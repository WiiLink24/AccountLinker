#pragma once

enum ExitType {
  POWER,
  WII_MENU,
};

void InitGUIThreads();
[[noreturn]] void MainMenu();

extern bool ExitRequested;
extern ExitType exitType;