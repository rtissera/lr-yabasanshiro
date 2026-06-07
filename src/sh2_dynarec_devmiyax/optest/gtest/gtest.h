// Minimal GoogleTest shim — just enough to compile devmiyax dynarec test/*.cpp
// Supports: ::testing::Test, TEST_F(fixture,name), EXPECT_EQ(a,b), RUN_ALL_TESTS().
#ifndef MINI_GTEST_H
#define MINI_GTEST_H
#include <cstdio>
#include <cstdint>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>

namespace testing {
class Test {
 public:
  virtual ~Test() {}
  virtual void SetUp() {}
  virtual void TearDown() {}
};
struct Case { const char* suite; const char* name; Test* (*make)(); void (*run)(Test*); };
inline std::vector<Case>& registry() { static std::vector<Case> r; return r; }
inline int g_fail_cases = 0;
inline int g_cur_fail = 0;
}  // namespace testing

#define GTEST_CLASS(suite,name) suite##_##name##_Test
#define GTEST_REG(suite,name)   suite##_##name##_Reg
#define TEST_F(suite,name)                                                      \
  class GTEST_CLASS(suite,name) : public suite {                                \
   public: void Body();                                                         \
    static ::testing::Test* Make() { return new GTEST_CLASS(suite,name)(); }    \
    static void Run(::testing::Test* t) {                                       \
      static_cast<GTEST_CLASS(suite,name)*>(t)->Body(); }                       \
  };                                                                            \
  struct GTEST_REG(suite,name) {                                                \
    GTEST_REG(suite,name)(){ ::testing::registry().push_back(                   \
      ::testing::Case{#suite,#name,&GTEST_CLASS(suite,name)::Make,              \
                      &GTEST_CLASS(suite,name)::Run}); }                        \
  } g_reg_##suite##_##name;                                                     \
  void GTEST_CLASS(suite,name)::Body()

namespace testing {
template<class A, class B>
inline void expect_eq(const A& a, const B& b, const char* sa, const char* sb,
                      const char* file, int line) {
  if (!(a == b)) {
    g_cur_fail++;
    std::ostringstream os;
    os << "    FAIL " << file << ":" << line << "  EXPECT_EQ(" << sa << ", "
       << sb << ")  exp=" << a << " got=" << b;
    std::cout << os.str() << "\n";
  }
}
}  // namespace testing
#define EXPECT_EQ(a,b) ::testing::expect_eq((a),(b),#a,#b,__FILE__,__LINE__)

#define EXPECT_TRUE(a)  EXPECT_EQ(true,(bool)(a))
#define EXPECT_FALSE(a) EXPECT_EQ(false,(bool)(a))
#define ASSERT_EQ(a,b)  EXPECT_EQ(a,b)

inline int RUN_ALL_TESTS() {
  using namespace testing;
  int total=0;
  for (auto& c : registry()) {
    total++;
    std::fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {                 // child: run the test in isolation
      g_cur_fail = 0;
      Test* t = c.make();
      t->SetUp();
      c.run(t);
      t->TearDown();
      delete t;
      std::fflush(stdout);
      _exit(g_cur_fail ? 1 : 0);
    }
    int st = 0; waitpid(pid, &st, 0);
    if (WIFSIGNALED(st)) {
      g_fail_cases++;
      std::printf("[ CRASH] %s.%s (signal %d)\n", c.suite, c.name, WTERMSIG(st));
    } else if (WEXITSTATUS(st)) {
      g_fail_cases++;
      std::printf("[ FAIL ] %s.%s\n", c.suite, c.name);
    } else {
      std::printf("[  ok  ] %s.%s\n", c.suite, c.name);
    }
  }
  std::printf("\n==== %d cases, %d failed ====\n", total, g_fail_cases);
  return g_fail_cases ? 1 : 0;
}

#endif
