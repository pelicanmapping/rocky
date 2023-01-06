/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Status.h"

using namespace rocky;

const rocky::Status rocky::StatusOK;

std::string rocky::Status::_errorCodeText[6] = {
    "No error",
    "Resource unavailable",
    "Service unavailable",
    "Configuration error",
    "Assertion failure",
    "Error"
};
