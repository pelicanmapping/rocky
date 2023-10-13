/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Status.h"

using namespace ROCKY_NAMESPACE;

const rocky::Status rocky::StatusOK;
const rocky::Status rocky::StatusError{ rocky::Status::GeneralError };

std::string rocky::Status::_errorCodeText[6] = {
    "No error",
    "Resource unavailable",
    "Service unavailable",
    "Configuration error",
    "Assertion failure",
    "Error"
};
