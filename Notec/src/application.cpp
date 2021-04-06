#include "engine/application.h"
#include "engine/engine.h"
#include "graphics/rhi/window.h"
#include "core/memory/linear_allocator.h"
#include "ui/ui.h"
#include "ui/draw.h"

#include "graphics/rhi/rhi.h"
#include "graphics/renderer/renderer.h"

#include "engine/input.h"
#include "core/time.h"

#include "lexer.h"
#include "ast.h"
#include "gap.h"
#include "code_block.h"

struct Notec {
    UI* ui;
    UIRenderer* ui_renderer;
    
    GapBuffer gap;
    
    Lexer* lexer;
    CodeBlockState state;
    
    AstPool* ast_pool;
    AstModule* ast_module;
    
    AST* cursor;
    
    float font_size = 15;
};

void set_theme(UITheme& theme) {
    theme
    .color(ThemeColor::Text, white)
    .color(ThemeColor::Button, shade3)
    .color(ThemeColor::ButtonHover, shade3)
    .color(ThemeColor::Panel, shade1)
    .color(ThemeColor::Splitter, shade1)
    .color(ThemeColor::SplitterHover, blue)
    .color(ThemeColor::Input, shade1)
    .color(ThemeColor::InputHover, shade3)
    .color(ThemeColor::Cursor, blue)
    .size(ThemeSize::DefaultFontSize, 10)
    .size(ThemeSize::Title1FontSize, 13)
    .size(ThemeSize::Title2FontSize, 12)
    .size(ThemeSize::Title3FontSize, 11)
    .size(ThemeSize::SplitterThickness, 1)
    .size(ThemeSize::VStackMargin, 0)
    .size(ThemeSize::HStackMargin, 0)
    .size(ThemeSize::VStackPadding, 4)
    .size(ThemeSize::HStackPadding, 0)
    .size(ThemeSize::StackSpacing, 4)
    .size(ThemeSize::VecSpacing, 2)
    .size(ThemeSize::TextPadding, 0)
    .size(ThemeSize::InputPadding, 4)
    .size(ThemeSize::InputMinWidth, 60)
    .size(ThemeSize::InputMaxWidth, {Perc, 20});
}

APPLICATION_API Notec* init(void* args, Modules& engine) {
    Notec* notec = PERMANENT_ALLOC(Notec);
    notec->ui = make_ui();
    notec->ui_renderer = make_ui_renderer();
    notec->lexer = make_lexer();
    notec->ast_pool = make_ast_pool();
    notec->ast_module = make_ast_module(notec->ast_pool);
    notec->state.lexer = notec->lexer;
    
    init_lexer_tables();
    
    load_font(*notec->ui, "fonts/RobotoMono-VariableFont_wght.ttf");
    set_theme(get_ui_theme(*notec->ui));
    
    //todo move out of application init and into engine init
    Dependency dependencies[1] = {
        { FRAGMENT_STAGE, RenderPass::Scene },
    };
    
    build_framegraph(*engine.renderer, { dependencies, 1});
    end_gpu_upload();

    return notec;
}

APPLICATION_API void reload(Notec& app, Modules& modules) {
    init_lexer_tables();
}

APPLICATION_API bool is_running(Notec& app, Modules& modules) {
    return !modules.window->should_close();
}

slice<Token> tokens;

APPLICATION_API void update(Notec& app, Modules& engine) {
    Input& input = *engine.input;
}

APPLICATION_API void render(Notec& app, Modules& engine) {
    UI& ui = *app.ui;
    
    RenderPass screen = begin_render_frame();
    
    ScreenInfo info = get_screen_info(*engine.window);
    begin_ui_frame(ui, info, *engine.input, engine.window->cursor_shape);
    
    begin_vstack(ui,0).background(shade1);

    text(ui, "main.top").padding(2.5).background(shade2);
    divider(ui, shade2);

    begin_scroll_view(ui);
    code_block(ui, &app.state).padding(5);
    end_scroll_view(ui);

    end_vstack(ui);
    
    end_ui_frame(ui);
    
    
    submit_draw_data(*app.ui_renderer, screen.cmd_buffer, get_ui_draw_data(ui));
    
    
    end_render_frame(screen);
}

APPLICATION_API void deinit(Notec* app, Modules& engine) {
    
}


