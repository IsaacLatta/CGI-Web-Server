#include <gtest/gtest.h>

#include <asio.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/use_future.hpp>

#include "testkit/utils.h"

#include "http/mw//Middleware.h"

#include "core/dbg.h"

namespace {

struct TestContext {
    std::vector<int> calls;
    int finish_count { 0 };
};

class RecordingMiddleware : public mw::Middleware<TestContext> {
public:
    explicit RecordingMiddleware(int id) : id_(id) {}

    asio::awaitable<void> Process(TestContext& ctx, Next next, Finish) override {
        ctx.calls.push_back(id_);
        co_return co_await next(ctx);
    }

private:
    int id_;
};

class TerminalMiddleware : public mw::Middleware<TestContext> {
public:
    explicit TerminalMiddleware(int id) : id_(id) {}

    asio::awaitable<void> Process(TestContext& ctx, Next, Finish) override {
        ctx.calls.push_back(id_);
        co_return;
    }

private:
    int id_;
};

class FinishingMiddleware : public mw::Middleware<TestContext> {
public:
    explicit FinishingMiddleware(int id) : id_(id) {}

    asio::awaitable<void> Process(TestContext& ctx, Next, Finish finish) override {
        ctx.calls.push_back(id_);
        co_return co_await finish(ctx, http::Response{}, std::nullopt);
    }

private:
    int id_;
};

class FinishThenNextMiddleware : public mw::Middleware<TestContext> {
public:
    explicit FinishThenNextMiddleware(int id) : id_(id) {}

    asio::awaitable<void> Process(TestContext& ctx, Next next, Finish finish) override {
        ctx.calls.push_back(id_);
        co_await finish(ctx, http::Response{}, std::nullopt);
        co_return co_await next(ctx);
    }

private:
    int id_;
};

mw::FinishCallback<TestContext> finish_callback() {
    return [](TestContext& ctx, http::Response, std::optional<http::ResponseHandler>) -> asio::awaitable<void> {
        ++ctx.finish_count;
        co_return;
    };
}

} // namespace

TEST(MiddlewarePipelineTest, EmptyPipelineContinues) {
    mw::Pipeline<TestContext> pipeline;
    TestContext ctx;

    auto outcome = testkit::run_awaitable(pipeline.Run(ctx, finish_callback()));

    EXPECT_EQ(outcome, mw::PipelineOutcome::Continued);
    EXPECT_TRUE(ctx.calls.empty());
    EXPECT_EQ(ctx.finish_count, 0);
}

TEST(MiddlewarePipelineTest, InvokesMiddlewareInOrder) {
    mw::Pipeline<TestContext> pipeline;
    TestContext ctx;

    pipeline
        .AddComponent<RecordingMiddleware>(1)
        .AddComponent<RecordingMiddleware>(2)
        .AddComponent<RecordingMiddleware>(3);

    auto outcome = testkit::run_awaitable(pipeline.Run(ctx, finish_callback()));

    EXPECT_EQ(outcome, mw::PipelineOutcome::Continued);
    EXPECT_EQ(ctx.calls, std::vector<int>({1, 2, 3}));
    EXPECT_EQ(ctx.finish_count, 0);
}

TEST(MiddlewarePipelineTest, MiddlewareCanStopByNotCallingNext) {
    mw::Pipeline<TestContext> pipeline;
    TestContext ctx;

    pipeline
        .AddComponent<RecordingMiddleware>(1)
        .AddComponent<TerminalMiddleware>(2)
        .AddComponent<RecordingMiddleware>(3);

    auto outcome = testkit::run_awaitable(pipeline.Run(ctx, finish_callback()));

    EXPECT_EQ(outcome, mw::PipelineOutcome::Continued);
    EXPECT_EQ(ctx.calls, std::vector<int>({1, 2}));
    EXPECT_EQ(ctx.finish_count, 0);
}

TEST(MiddlewarePipelineTest, FinishSetsOutcomeToFinished) {
    mw::Pipeline<TestContext> pipeline;
    TestContext ctx;

    pipeline
        .AddComponent<RecordingMiddleware>(1)
        .AddComponent<FinishingMiddleware>(2)
        .AddComponent<RecordingMiddleware>(3);

    auto outcome = testkit::run_awaitable(pipeline.Run(ctx, finish_callback()));

    EXPECT_EQ(outcome, mw::PipelineOutcome::Finished);
    EXPECT_EQ(ctx.calls, std::vector<int>({1, 2}));
    EXPECT_EQ(ctx.finish_count, 1);
}

TEST(MiddlewarePipelineTest, FinishDoesNotAutomaticallyPreventNext) {
    mw::Pipeline<TestContext> pipeline;
    TestContext ctx;

    core::dbg::AssertionGuard guard;

    pipeline
        .AddComponent<RecordingMiddleware>(1)
        .AddComponent<FinishThenNextMiddleware>(2)
        .AddComponent<RecordingMiddleware>(3);

    auto outcome = testkit::run_awaitable(pipeline.Run(ctx, finish_callback()));

    EXPECT_EQ(outcome, mw::PipelineOutcome::Finished);
    EXPECT_EQ(ctx.calls, std::vector<int>({1, 2}));
    EXPECT_EQ(ctx.finish_count, 1);
}