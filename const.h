// Copyright © 2015, The Network Protocol Company, Inc. All Rights Reserved.

#ifndef CONST_H
#define CONST_H

static const int ServerFramesPerSecond = 30;
static const int ClientFramesPerSecond = 60;

static const int TicksPerSecond = 240;

static const int TicksPerClientFrame = TicksPerSecond / ClientFramesPerSecond;
static const int TicksPerServerFrame = TicksPerSecond / ServerFramesPerSecond;

static const double ServerFrameDeltaTime = 1.0 / ServerFramesPerSecond;
static const double ClientFrameDeltaTime = 1.0 / ServerFramesPerSecond;

static const double ServerFrameSafety = 0.5;

static const double AverageSleepJitter = 2.25 * 0.001;

static const int MaxEntities = 1024;
static const int MaxPlayers = 2;
static const int MaxCubes = MaxEntities;

static const int MaxContexts = 8;
static const int MaxPacketSize = 4 * 1024;
static const int UnitsPerMeter = 512;

static const int OrientationBits = 9;
static const int PositionBoundXY = 32;
static const int PositionBoundZ = 16;

static const int QuantizedPositionBoundXY = UnitsPerMeter * PositionBoundXY - 1;
static const int QuantizedPositionBoundZ = UnitsPerMeter * PositionBoundZ - 1;

#endif // #ifndef CONST_H
