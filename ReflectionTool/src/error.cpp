//
//  error.cpp
//  TopLangCompiler
//
//  Created by Lucas Goetz on 20/09/2019.
//  Copyright © 2019 Lucas Goetz. All rights reserved.
//

#include "error.h"
#include <stdio.h>
#include "core/container/tvector.h"
#include "core/container/array.h"
#include "helper.h"
#include "core/container/string_view.h"

using string = string_view;

namespace pixc {
    namespace error {
        void terminate_line(string& line, unsigned int start, unsigned int end) {
            line.length = end - start;
        }
        
        void to_cstr_slice(char* buffer, string line, unsigned int start, unsigned int end) {
            assert(start <= end);
            
            line.data += start;
            line.length = end - start;
            to_cstr(line, buffer, 300);
        }
        
        void log_error(Error* error) {
            int first_begin_line = max(error->line - 2, 1);
            int end_line = error->line + 3;
            
            int begin_line = first_begin_line;
            
            int begin_i = 0;
            
            int line = 1;
            
            array<6, string> lines = {};
            bool searching_for_end_of_line = false;
            
            for (int i = 0; i < error->src.length; i++) {
                if (error->src[i] == '\n') line++;
                
                if (line == begin_line) {
                    if (begin_line == end_line) {
                        terminate_line(lines.last(), begin_i, i);
                        searching_for_end_of_line = false;
                        break;
                    }
                    
                    if (begin_line != first_begin_line) {
                        terminate_line(lines.last(), begin_i, i);
                    }
                    
                    int offset = line == 1 ? 0 : 1;
                    lines.append({error->src.data + i + offset, 0});
                    begin_i = i + offset;
                    begin_line++;
                    
                    searching_for_end_of_line = true;
                }
    
            }
        
            if (searching_for_end_of_line) {
                terminate_line(lines.last(), begin_i, error->src.length);
            }
            
            char mesg[100], filename[100];
            to_cstr(error->mesg, mesg, 100);
            to_cstr(error->filename, filename, 100);
            
            const char* group = "";
            switch (error->id) {
                case IndentationError: group = "IndentationError"; break;
                case SyntaxError: group = "SyntaxError"; break;
				case SemanticError: group = "SemanticError"; break;
                case UnknownToken: group = "LexingError"; break;
                case RedefinitionError: group = "RedefinitionError"; break;
                case UnknownVariable: group = "UnknownVariableError"; break;
                case TypeError: group = "TypeError"; break;
            }
            
            printf("%sError[%i]%s : %s \n --> %s:%i:%i\n\n", red, error->id, black, group, filename, error->line, error->column);
            
            for (int i = 0; i < lines.length; i++) { //todo better formatting for
                unsigned int current_line = first_begin_line + i;
                
                if (current_line == error->line) printf(" | \t\n");
                
                if (current_line == error->line) {
                    char buffer[300];
                    
                    printf("%i| \t", current_line);
                    
                    int column_within_line = (int)lines[i].length - (int)error->column - (int)error->token_length;
                    
                    if (column_within_line >= 0) {
                        string line = lines[i];
                        to_cstr_slice(buffer, line, 0, error->column);
                        printf("%s", buffer);
                        
                        printf(red);
                        to_cstr_slice(buffer, line, error->column, error->column + error->token_length);
                        printf("%s", buffer);
                        printf(black);
                        
                        to_cstr_slice(buffer, line, error->column + error->token_length, line.length);
                    
                        printf("%s", buffer);
                    } else {
                        to_cstr(lines[i], buffer, 300);
                        printf("%s", buffer);
                    }
                
                    printf("\n |\t%.*s", error->column, "                   ");
                    printf(red);
                    printf("%.*s\n", max(error->token_length, 1), "^^^^^^^^^^^^^^^^^^^^^^");
                    printf("\nError[%i]%s : %s\n\n", error->id, black, mesg);
                } else {
                    char buffer[300];
                    to_cstr(lines[i], buffer, 300);
                    
                    printf("%i| \t%s\n", current_line, buffer);
                }
            }
            
            printf("\n");
        }
    
    }
}
