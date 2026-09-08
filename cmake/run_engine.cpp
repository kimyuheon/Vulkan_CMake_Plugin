// run_engine — IDE 의 Run 버튼이 누를 것을 만들어 주는 얇은 런처.
//
// ⭐ 왜 필요한가: 플러그인은 실행 파일이 아니라 라이브러리다. 그래서 IDE 에는 실행할
//    타겟이 없고, 개발자가 "Custom Executable" 을 손으로 만들어야 F5 가 된다. 그 설정은
//    .user 파일에 들어가는데, 그건 PC 마다 다른 파일이라 **버전 관리로 공유되지 않는다.**
//    → 실행 타겟을 소스로 두면 어느 PC 에서 클론해도 설정 없이 바로 실행된다.
//
// 하는 일은 두 줄이다:
//   1) 엔진 폴더로 chdir  — 엔진은 에셋도 plugins/ 도 **현재 작업 디렉터리** 기준으로 연다
//   2) 엔진으로 exec      — 프로세스를 갈아치운다. 껍데기가 남지 않는다
//
// ⚠️ exec 를 쓰는 이유가 디버깅이다. 자식 프로세스를 새로 띄우면 디버거는 이 런처에
//    붙어 있고 엔진은 놓친다. exec 는 **같은 프로세스**를 갈아치우므로 디버거가 그대로
//    따라오고, 플러그인의 중단점은 엔진이 dlopen 하는 순간 붙는다(pending → 해제).
//
// ⚠️ Windows 는 사정이 다르다. _execv 는 POSIX 처럼 프로세스를 갈아치우지 않고 부모를
//    끝낸 뒤 새 프로세스를 만든다 — 실행은 되지만 **디버거가 못 따라온다.** 거기서는
//    Custom Executable(실행 파일 = VulkanApp, 작업 디렉터리 = 엔진 폴더)을 쓰는 편이 낫다.

#include <cstdio>

#if defined(_WIN32)
    #include <direct.h>
    #include <process.h>
    #define chdir _chdir
    #define execv _execv
#else
    #include <unistd.h>
#endif

int main(int argc, char** argv) {
    (void)argc;
    if (chdir(VULKANCAD_RUN_DIR) != 0) {
        std::fprintf(stderr, "[run_engine] 작업 디렉터리로 못 갑니다: %s\n", VULKANCAD_RUN_DIR);
        return 1;
    }
    std::printf("[run_engine] %s 에서 %s 실행\n", VULKANCAD_RUN_DIR, VULKANCAD_ENGINE_EXE);
    std::fflush(stdout);

    // argv[0] 만 엔진 경로로 바꿔 넘긴다 — 나머지 인자는 그대로 통과.
    argv[0] = const_cast<char*>(VULKANCAD_ENGINE_EXE);
    execv(VULKANCAD_ENGINE_EXE, argv);

    std::perror("[run_engine] execv");   // 여기 오면 실패한 것이다
    return 1;
}
