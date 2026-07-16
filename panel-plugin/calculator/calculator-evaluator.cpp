/*
 * Copyright (C) 2026 Matteo Bonanomi
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "calculator-evaluator.h"

#include "calculator-output.h"

#include <gio/gio.h>

#include <algorithm>
#include <cstring>
#include <vector>

using namespace WhiskerMenu;

namespace
{

constexpr gsize kReadChunk = 4096;
constexpr gsize kMaximumCombinedOutput = 16384;

enum class JobTerminalReason
{
	None,
	Cancelled,
	TimedOut,
	Overflow,
	IoFailure
};

}

struct WhiskerMenu::CalculatorEvaluationJob
{
	CalculatorEvaluator* owner;
	CalculatorEvaluationRequest request;
	CalculatorEvaluator::Callback callback;
	GSubprocess* process;
	GCancellable* io_cancellable;
	guint timeout_source;
	std::string stdin_payload;
	std::string stdout_text;
	std::string stderr_text;
	JobTerminalReason terminal_reason;
	bool stdout_done;
	bool stderr_done;
	bool stdin_done;
	bool wait_done;
	bool silenced;
};

namespace
{

struct ReadContext
{
	CalculatorEvaluationJob* job;
	bool stdout_stream;
};

void maybe_finish(CalculatorEvaluationJob* job);

/* terminate_job:
 * @job: active evaluation that owns the child and pipe cancellation.
 * @reason: first terminal reason to retain for the final callback.
 *
 * Marks the request terminal, stops pipe IO, and force-exits the child. The
 * uncancellable wait callback remains active and owns final reaping/cleanup.
 */
void terminate_job(CalculatorEvaluationJob* job, JobTerminalReason reason)
{
	if (job->terminal_reason == JobTerminalReason::None)
		job->terminal_reason = reason;
	if (job->process && !job->wait_done)
	{
		g_subprocess_force_exit(job->process);
	}
	if (job->io_cancellable)
		g_cancellable_cancel(job->io_cancellable);
}

gboolean on_timeout(gpointer data)
{
	auto* job = static_cast<CalculatorEvaluationJob*>(data);
	job->timeout_source = 0;
	// HACK: GSubprocess has no compositor-independent deadline primitive.
	// Force-exit plus cancelled pipe IO keeps GTK responsive; the uncancellable
	// wait below still reaps the process before releasing the job.
	terminate_job(job, JobTerminalReason::TimedOut);
	return G_SOURCE_REMOVE;
}

void read_next(ReadContext* context);

void on_read(GObject* source, GAsyncResult* result, gpointer data)
{
	auto* context = static_cast<ReadContext*>(data);
	CalculatorEvaluationJob* job = context->job;
	GError* error = nullptr;
	GBytes* bytes = g_input_stream_read_bytes_finish(
			G_INPUT_STREAM(source), result, &error);
	if (error)
	{
		if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)
				&& job->terminal_reason == JobTerminalReason::None)
		{
			terminate_job(job, JobTerminalReason::IoFailure);
		}
		g_clear_error(&error);
		if (context->stdout_stream)
			job->stdout_done = true;
		else
			job->stderr_done = true;
		delete context;
		maybe_finish(job);
		return;
	}

	gsize size = 0;
	const gchar* chunk = static_cast<const gchar*>(g_bytes_get_data(bytes, &size));
	if (size == 0)
	{
		if (context->stdout_stream)
			job->stdout_done = true;
		else
			job->stderr_done = true;
		g_bytes_unref(bytes);
		delete context;
		maybe_finish(job);
		return;
	}

	std::string& output = context->stdout_stream
			? job->stdout_text : job->stderr_text;
	if (job->stdout_text.size() + job->stderr_text.size() + size
			> kMaximumCombinedOutput)
	{
		g_bytes_unref(bytes);
		terminate_job(job, JobTerminalReason::Overflow);
		if (context->stdout_stream)
			job->stdout_done = true;
		else
			job->stderr_done = true;
		delete context;
		maybe_finish(job);
		return;
	}
	output.append(chunk, size);
	g_bytes_unref(bytes);
	read_next(context);
}

void read_next(ReadContext* context)
{
	CalculatorEvaluationJob* job = context->job;
	GInputStream* stream = context->stdout_stream
			? g_subprocess_get_stdout_pipe(job->process)
			: g_subprocess_get_stderr_pipe(job->process);
	g_input_stream_read_bytes_async(stream, kReadChunk, G_PRIORITY_DEFAULT,
			job->io_cancellable, &on_read, context);
}

void on_stdin_closed(GObject* source, GAsyncResult* result, gpointer data)
{
	auto* job = static_cast<CalculatorEvaluationJob*>(data);
	GError* error = nullptr;
	g_output_stream_close_finish(G_OUTPUT_STREAM(source), result, &error);
	g_clear_error(&error);
	job->stdin_done = true;
	maybe_finish(job);
}

void on_stdin_written(GObject* source, GAsyncResult* result, gpointer data)
{
	auto* job = static_cast<CalculatorEvaluationJob*>(data);
	gsize written = 0;
	GError* error = nullptr;
	if (!g_output_stream_write_all_finish(G_OUTPUT_STREAM(source), result,
			&written, &error) && !g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
	{
		terminate_job(job, JobTerminalReason::IoFailure);
	}
	g_clear_error(&error);
	// Closing stdin is a lifecycle transition: bc evaluates the complete final
	// line only after it sees the newline and EOF supplied by this adapter.
	g_output_stream_close_async(G_OUTPUT_STREAM(source), G_PRIORITY_DEFAULT,
			nullptr, &on_stdin_closed, job);
}

void on_wait(GObject* source, GAsyncResult* result, gpointer data)
{
	auto* job = static_cast<CalculatorEvaluationJob*>(data);
	GError* error = nullptr;
	g_subprocess_wait_finish(G_SUBPROCESS(source), result, &error);
	if (error && job->terminal_reason == JobTerminalReason::None)
		job->terminal_reason = JobTerminalReason::IoFailure;
	g_clear_error(&error);
	job->wait_done = true;
	maybe_finish(job);
}

void maybe_finish(CalculatorEvaluationJob* job)
{
	if (!job->wait_done || !job->stdout_done || !job->stderr_done
			|| !job->stdin_done)
		return;
	if (job->timeout_source)
	{
		g_source_remove(job->timeout_source);
		job->timeout_source = 0;
	}

	CalculatorEvaluationState state = CalculatorEvaluationState::Failed;
	std::string value;
	if (job->terminal_reason == JobTerminalReason::TimedOut)
		state = CalculatorEvaluationState::TimedOut;
	else if (job->terminal_reason == JobTerminalReason::Cancelled)
		state = CalculatorEvaluationState::Cancelled;
	else if (job->terminal_reason == JobTerminalReason::None
			&& g_subprocess_get_successful(job->process)
			&& job->stderr_text.find_first_not_of(" \t\r\n") == std::string::npos
			&& calculator_normalize_output(job->stdout_text,
				job->request.maximum_decimals, value))
	{
		state = CalculatorEvaluationState::Success;
	}

	if (!job->silenced && job->callback)
	{
		job->callback({ state, job->request.engine, job->request.expression, value,
				job->request.maximum_decimals, job->request.generation });
	}
	if (job->owner)
		job->owner->finish(job);
	g_clear_object(&job->process);
	g_clear_object(&job->io_cancellable);
	delete job;
}

}

CalculatorEvaluator::CalculatorEvaluator() :
	m_job(nullptr)
{
}

/* ~CalculatorEvaluator:
 *
 * Silences a pending completion, terminates the process, and leaves the job
 * alive until the uncancellable wait has reaped the child.
 */
CalculatorEvaluator::~CalculatorEvaluator()
{
	if (m_job)
	{
		m_job->silenced = true;
		m_job->owner = nullptr;
		terminate_job(m_job, JobTerminalReason::Cancelled);
		m_job = nullptr;
	}
}

/* evaluate:
 * @engine: selected known engine.
 * @expression: already validated exact input.
 * @maximum_decimals: output precision ceiling.
 * @generation: search-state identity used by the owner to reject stale work.
 * @callback: runs on the GLib main context after the process is reaped.
 *
 * Starts one bounded asynchronous subprocess request. Replacing a request
 * cancels the previous child; no shell participates in command construction.
 */
void CalculatorEvaluator::evaluate(CalculatorEngine engine,
		const std::string& expression, int maximum_decimals,
		unsigned int generation, Callback callback)
{
	cancel();
	const std::string program_path = calculator_engine_resolve_path(engine);
	if (program_path.empty())
	{
		callback({ CalculatorEvaluationState::Unavailable, engine, expression,
				std::string(), maximum_decimals, generation });
		return;
	}
	if (!calculator_query_is_safe(expression)
			|| maximum_decimals < 0 || maximum_decimals > 10)
	{
		callback({ CalculatorEvaluationState::Failed, engine, expression,
				std::string(), maximum_decimals, generation });
		return;
	}

	const std::vector<std::string> arguments = calculator_engine_argv(engine,
			program_path, expression, maximum_decimals);
	std::vector<const gchar*> argv;
	for (const auto& argument : arguments)
		argv.push_back(argument.c_str());
	argv.push_back(nullptr);

	GSubprocessFlags flags = static_cast<GSubprocessFlags>(
			G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE);
	if (calculator_engine_descriptor(engine).input == CalculatorInput::StandardInput)
		flags = static_cast<GSubprocessFlags>(flags | G_SUBPROCESS_FLAGS_STDIN_PIPE);
	else
		flags = static_cast<GSubprocessFlags>(flags | G_SUBPROCESS_FLAGS_STDIN_INHERIT);

	GSubprocessLauncher* launcher = g_subprocess_launcher_new(flags);
	if (engine == CalculatorEngine::Bc)
		g_subprocess_launcher_setenv(launcher, "BC_LINE_LENGTH", "0", TRUE);
	GError* error = nullptr;
	GSubprocess* process = g_subprocess_launcher_spawnv(launcher, argv.data(), &error);
	g_object_unref(launcher);
	if (!process)
	{
		g_clear_error(&error);
		callback({ CalculatorEvaluationState::Failed, engine, expression,
				std::string(), maximum_decimals, generation });
		return;
	}

	CalculatorEvaluationRequest request = { engine, expression, program_path,
			maximum_decimals, generation };
	m_job = new CalculatorEvaluationJob { this, request, callback, process,
			g_cancellable_new(), 0,
			calculator_engine_stdin(engine, expression, maximum_decimals),
			std::string(), std::string(), JobTerminalReason::None,
			false, false,
			calculator_engine_descriptor(engine).input != CalculatorInput::StandardInput,
			false, false };
	CalculatorEvaluationJob* job = m_job;
	job->timeout_source = g_timeout_add(2000, &on_timeout, job);

	read_next(new ReadContext { job, true });
	read_next(new ReadContext { job, false });
	// The wait is intentionally uncancellable: cancellation stops IO and kills
	// the child, while this callback remains responsible for reaping it.
	g_subprocess_wait_async(process, nullptr, &on_wait, job);

	if (!job->stdin_payload.empty())
	{
		GOutputStream* stdin_pipe = g_subprocess_get_stdin_pipe(process);
		g_output_stream_write_all_async(stdin_pipe, job->stdin_payload.data(),
				job->stdin_payload.size(), G_PRIORITY_DEFAULT, job->io_cancellable,
				&on_stdin_written, job);
	}
}

void CalculatorEvaluator::cancel()
{
	if (!m_job)
		return;
	terminate_job(m_job, JobTerminalReason::Cancelled);
}

void CalculatorEvaluator::finish(CalculatorEvaluationJob* job)
{
	if (m_job == job)
		m_job = nullptr;
}
