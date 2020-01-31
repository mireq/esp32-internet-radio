#pragma once

#define traceTASK_SWITCHED_IN() logTaskSwitchedIn(pxCurrentTCB);
#define traceTASK_SWITCHED_OUT() logTaskSwitchedOut(pxCurrentTCB);
