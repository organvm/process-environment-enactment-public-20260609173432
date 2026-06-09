#define UNICODE
#define _UNICODE
#include <windows.h>
#include <stdio.h>
#include <wchar.h>

static int print_last_error(const wchar_t *where) {
  DWORD code = GetLastError();
  fwprintf(stderr, L"%ls failed: %lu\n", where, code);
  return 1;
}

static int expect_value(const wchar_t *name, const wchar_t *expected) {
  wchar_t actual[256];
  DWORD length = GetEnvironmentVariableW(name, actual, 256);
  if (length == 0 || length >= 256 || wcscmp(actual, expected) != 0) {
    fwprintf(stderr, L"expected %ls=%ls\n", name, expected);
    return 1;
  }
  return 0;
}

static int expect_absent(const wchar_t *name) {
  wchar_t actual[2];
  DWORD length = GetEnvironmentVariableW(name, actual, 2);
  if (length != 0) {
    fwprintf(stderr, L"expected %ls to be absent\n", name);
    return 1;
  }
  return 0;
}

static int child_mode(const wchar_t *label) {
  int status = 0;
  LPWCH block = GetEnvironmentStringsW();
  if (block == NULL) {
    return print_last_error(L"GetEnvironmentStringsW");
  }

  wprintf(L"CASE:%ls\n", label);
  for (LPWCH cursor = block; *cursor != L'\0'; cursor += wcslen(cursor) + 1) {
    if (wcsncmp(cursor, L"PM_", 3) == 0) {
      wprintf(L"%ls\n", cursor);
    }
  }

  if (wcscmp(label, L"inherit-null-lpEnvironment") == 0) {
    status |= expect_value(L"PM_INHERITED", L"parent");
  } else if (wcscmp(label, L"custom-base-block") == 0) {
    status |= expect_absent(L"PM_INHERITED");
    status |= expect_value(L"PM_FIRST", L"1");
    status |= expect_value(L"PM_SECOND", L"2");
    status |= expect_value(L"PM_THIRD", L"3");
  } else if (wcscmp(label, L"custom-append-block") == 0) {
    status |= expect_absent(L"PM_INHERITED");
    status |= expect_value(L"PM_ADDED", L"entry");
    status |= expect_value(L"PM_FIRST", L"1");
    status |= expect_value(L"PM_SECOND", L"2");
    status |= expect_value(L"PM_THIRD", L"3");
  } else if (wcscmp(label, L"custom-omit-block") == 0) {
    status |= expect_absent(L"PM_INHERITED");
    status |= expect_absent(L"PM_FIRST");
    status |= expect_value(L"PM_SECOND", L"2");
    status |= expect_value(L"PM_THIRD", L"3");
  } else if (wcscmp(label, L"custom-replace-block") == 0) {
    status |= expect_absent(L"PM_INHERITED");
    status |= expect_value(L"PM_FIRST", L"replaced");
    status |= expect_value(L"PM_SECOND", L"2");
    status |= expect_value(L"PM_THIRD", L"3");
  } else {
    fwprintf(stderr, L"unknown label: %ls\n", label);
    status = 1;
  }

  if (!FreeEnvironmentStringsW(block)) {
    return print_last_error(L"FreeEnvironmentStringsW");
  }
  return status;
}

static int run_case(const wchar_t *exe, const wchar_t *label, wchar_t *environment_block) {
  wchar_t command_line[32768];
  STARTUPINFOW startup;
  PROCESS_INFORMATION process;
  DWORD flags = environment_block == NULL ? 0 : CREATE_UNICODE_ENVIRONMENT;

  ZeroMemory(&startup, sizeof(startup));
  ZeroMemory(&process, sizeof(process));
  startup.cb = sizeof(startup);

  if (swprintf(command_line, 32768, L"\"%ls\" child %ls", exe, label) < 0) {
    fwprintf(stderr, L"command line construction failed\n");
    return 1;
  }

  if (!CreateProcessW(exe, command_line, NULL, NULL, FALSE, flags, environment_block, NULL, &startup, &process)) {
    return print_last_error(L"CreateProcessW");
  }

  WaitForSingleObject(process.hProcess, INFINITE);

  DWORD exit_code = 1;
  if (!GetExitCodeProcess(process.hProcess, &exit_code)) {
    print_last_error(L"GetExitCodeProcess");
  }

  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return (int)exit_code;
}

int wmain(int argc, wchar_t **argv) {
  if (argc >= 3 && wcscmp(argv[1], L"child") == 0) {
    return child_mode(argv[2]);
  }

  wchar_t exe[MAX_PATH];
  if (GetModuleFileNameW(NULL, exe, MAX_PATH) == 0) {
    return print_last_error(L"GetModuleFileNameW");
  }

  if (!SetEnvironmentVariableW(L"PM_INHERITED", L"parent")) {
    return print_last_error(L"SetEnvironmentVariableW");
  }

  wchar_t base_block[] = L"PM_FIRST=1\0" L"PM_SECOND=2\0" L"PM_THIRD=3\0" L"\0";
  wchar_t append_block[] = L"PM_ADDED=entry\0" L"PM_FIRST=1\0" L"PM_SECOND=2\0" L"PM_THIRD=3\0" L"\0";
  wchar_t omit_block[] = L"PM_SECOND=2\0" L"PM_THIRD=3\0" L"\0";
  wchar_t replace_block[] = L"PM_FIRST=replaced\0" L"PM_SECOND=2\0" L"PM_THIRD=3\0" L"\0";

  int status = 0;
  status |= run_case(exe, L"inherit-null-lpEnvironment", NULL);
  status |= run_case(exe, L"custom-base-block", base_block);
  status |= run_case(exe, L"custom-append-block", append_block);
  status |= run_case(exe, L"custom-omit-block", omit_block);
  status |= run_case(exe, L"custom-replace-block", replace_block);
  return status;
}
