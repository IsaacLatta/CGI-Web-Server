#include <functional>
#include <string>

#include <gtest/gtest.h>

#include "core/Resolver.h"

namespace {

    using IntFactory = std::function<int()>;
    using ResolverAwareFactory = std::function<std::string(const core::Resolver&, int)>;

TEST(Resolver, GetOrReturnsBoundFactoryInsteadOfFallback) {
    core::Resolver resolver;
    resolver.Set<IntFactory>([] { return 42; });

    auto factory = resolver.GetOr<IntFactory>([] { return -1; });

    EXPECT_EQ(factory(), 42);
}

TEST(Resolver, GetOrUsesFallbackWhenFactoryNotBound) {
    core::Resolver resolver;

    auto factory = resolver.GetOr<IntFactory>([] { return 7; });

    ASSERT_TRUE(factory);
    EXPECT_EQ(factory(), 7);
}

TEST(Resolver, GetOrAndInvokePassesResolverWhenFactoryAcceptsIt) {
    core::Resolver resolver;
    resolver.Set<ResolverAwareFactory>(
        [](const core::Resolver&, int value) {
            return std::to_string(value * 2);
        });

    const std::string result =
        resolver.GetOrAndInvoke<ResolverAwareFactory>(
            [](const core::Resolver&, int value) {
                return std::to_string(value);
            }, 21);

    EXPECT_EQ(result, "42");
}

TEST(Resolver, SetReplacesExistingBindingForSameAlias) {
    core::Resolver resolver;
    resolver.Set<IntFactory>([] { return 1; });
    resolver.Set<IntFactory>([] { return 99; });

    auto factory = resolver.GetOr<IntFactory>([] { return -1; });

    ASSERT_TRUE(factory);
    EXPECT_EQ(factory(), 99);
}

}