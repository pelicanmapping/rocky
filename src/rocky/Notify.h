/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <iostream>

#define ROCKY_DEBUG if (false) std::cout << "[rocky]--"
#define ROCKY_INFO std::cout << "[rocky]- "
#define ROCKY_NOTICE std::cout << "[rocky]  "
#define ROCKY_WARN std::cout << "[rocky]* "
#define ROCKY_NULL if (false) std::cout

#define ROCKY_DEPRECATED(A, B) OE_WARN << #A << " is deprecated; please use " << #B << std::endl

#if defined(_MSC_VER)
#define ROCKY_FILE (std::strrchr(__FILE__, '\\') ? std::strrchr(__FILE__, '\\') + 1 : __FILE__)
#else
#define ROCKY_FILE (std::strrchr(__FILE__, '/') ? std::strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#define ROCKY_SOFT_ASSERT(EXPR, ...) if(!(EXPR)) { ROCKY_WARN << "ASSERTION FAILURE (" << __func__ << " @ " << ROCKY_FILE << ":" << __LINE__ << ") " #EXPR " ..." << __VA_ARGS__ "" << std::endl; }
#define ROCKY_SOFT_ASSERT_AND_RETURN(EXPR, RETVAL, ...) if(!(EXPR)) { ROCKY_WARN << "ASSERTION FAILURE (" << __func__ << " @ " << ROCKY_FILE << ":" << __LINE__ << ") " #EXPR " ..." << __VA_ARGS__ "" << std::endl; return RETVAL; }
#define ROCKY_IF_SOFT_ASSERT(EXPR, ...) if(!(EXPR)) { ROCKY_WARN << "ASSERTION FAILURE (" << __func__ << " @ " << ROCKY_FILE << ":" << __LINE__ << ") " #EXPR " ..." << __VA_ARGS__ "" << std::endl; } else
#define ROCKY_HARD_ASSERT(EXPR, ...) if(!(EXPR)) { ROCKY_WARN << "FATAL ASSERTION FAILURE (" << __func__ << " @ " << ROCKY_FILE << ":" << __LINE__ << ") " #EXPR " ..." << __VA_ARGS__ "" << std::endl; abort(); }

#define ROCKY_TODO(...) ROCKY_WARN << "TODO (" << __func__ << " @ " << ROCKY_FILE << ":" << __LINE__ << ")..." << __VA_ARGS__ "" << std::endl