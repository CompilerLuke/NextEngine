//
//  error.hpp
//  TopLangCompiler
//
//  Created by Lucas Goetz on 20/09/2019.
//  Copyright © 2019 Lucas Goetz. All rights reserved.
//

#pragma once

#include "core/container/string_view.h"
#include "core/memory/linear_allocator.h"

using string = string_view;

namespace pixc {
    namespace error {
        enum ErrorID { UnknownToken, SyntaxError, SemanticError, IndentationError, RedefinitionError, UnknownVariable, TypeError };
        
        struct Error {
			struct LinearAllocator* allocator;

            string mesg;
            
            string filename;
            string src;
            
            unsigned int line = 0;
            unsigned int column = 0;
            unsigned int token_length = 0;
            
            ErrorID id;
        };
        
        inline bool is_error(Error* error) { return error->mesg.length > 0; }
        
        void log_error(Error* error);

#ifdef __APPLE__
		constexpr const char* red = "\033[1;31m";
		constexpr const char* black = "\033[0m";
#else
		constexpr const char* red = "";
		constexpr const char* black = "";
#endif
    }
}
