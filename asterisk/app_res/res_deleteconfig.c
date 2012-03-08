/*
 * deleteconfig Application For Asterisk
 *
 * Copyright (c) 2010-2011 Sebastiano Merlino <etr@pensieroartificiale.com>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "asterisk.h"
#include "asterisk/paths.h"

#include <stdio.h> 
#include <asterisk/file.h>
#include <asterisk/logger.h>
#include <asterisk/channel.h>
#include <asterisk/pbx.h>
#include <asterisk/module.h>
#include <asterisk/manager.h>
#include <asterisk/lock.h>
#include <asterisk/app.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static char *app = "DeleteConfig";
static char *synopsis = "Delete a config file.";
static char *desc = "Delete a config file";

static int action_deleteconfig(struct mansession *s, const struct message *m)
{
        const char *fn = astman_get_header(m, "Filename");
        struct ast_str *filepath = ast_str_alloca(PATH_MAX);
        ast_str_set(&filepath, 0, "%s/", ast_config_AST_CONFIG_DIR);
        ast_str_append(&filepath, 0, "%s", fn); 

        if (remove( ast_str_buffer(filepath) ) != 0) {
                astman_send_ack(s, m, "Configuration file deleted successfully");
        } else {
                astman_send_error(s, m, strerror(errno));
        }    

        return 0;
}

static int unload_module(void)
{
//    ast_custom_function_unregister(&backticks_function);
    ast_manager_unregister(app);
    return ast_unregister_application(app);
}

static int load_module(void)
{
//    ast_custom_function_register(&backticks_function);

    int res = ast_manager_register2(app, EVENT_FLAG_SYSTEM, action_deleteconfig, desc, synopsis);
    return res;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, "DeleteConfig Command",
		.load = load_module,
		.unload = unload_module,
);

