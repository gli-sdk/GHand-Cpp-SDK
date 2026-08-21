// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#ifndef INCLUDE_GHAND_EXPORT_H_
#define INCLUDE_GHAND_EXPORT_H_

#ifdef _WIN32
#ifdef GHAND_BUILD
#define GHAND_API __declspec(dllexport)
#else
#define GHAND_API __declspec(dllimport)
#endif
#else
#define GHAND_API
#endif

#endif  // INCLUDE_GHAND_EXPORT_H_
