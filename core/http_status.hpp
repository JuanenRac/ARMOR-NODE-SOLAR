// ARMOR-SOLAR - the status line of each answer the panel's server gives.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// One table for the firmware and for the tests: a code the panel sends that is missing here would go out as a 500 with the right body, which a browser reads as a server fault.
#pragma once
#include <string_view>

namespace armor::http {

// Every code the panel's routes answer with, in order.
inline constexpr int kKnownCodes[] = {200, 202, 400, 401, 403, 404, 408, 409, 413, 422, 423, 429, 500, 503};

inline const char* status_line(int code) {
  switch (code) {
    case 200: return "200 OK";
    case 202: return "202 Accepted";   // the work was started; the answer comes later (the BMS settings, the console)
    case 400: return "400 Bad Request";
    case 401: return "401 Unauthorized";
    case 403: return "403 Forbidden";
    case 404: return "404 Not Found";
    case 408: return "408 Request Timeout";
    case 409: return "409 Conflict";
    case 413: return "413 Payload Too Large";
    case 422: return "422 Unprocessable Entity";
    case 423: return "423 Locked";
    case 429: return "429 Too Many Requests";
    case 503: return "503 Service Unavailable";
    default: return "500 Internal Server Error";
  }
}

}  // namespace armor::http
