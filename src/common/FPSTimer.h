//
// Copyright (c) 2019 The Aquarium Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FPSTimer.h: Define fps timer.

#pragma once
#ifndef FPS_TIMER
#define FPS_TIMER 1

#include <vector>

constexpr int NUM_HISTORY_DATA = 100;

class FPSTimer
{
public:
  FPSTimer();

  void update(double renderingTime, int fpsCount, int logCount);
  double getAverageFPS() const { return mAverageFPS; }
  const float *getHistoryFps() const { return mHistoryFPS.data(); }
  const float *getHistoryFrameTime() const { return mHistoryFrameTime.data(); }
  std::vector<float> &getRecordFps() { return mRecordFps; }

private:
  std::vector<float> mHistoryFPS;
  std::vector<float> mHistoryFrameTime;
  std::vector<float> mRecordFps;
  int mRecordFpsFrequencyCursor;
  double mAverageFPS;
};

#endif