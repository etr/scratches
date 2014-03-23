/*
 * backticks Application For Asterisk
 *
 * Copyright (c) 2010-2011 Sebastiano Merlino <merlino.sebastiano@gmail.com>
 *
 * From an original idea of Anthony Minessale II
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

static char *app = "BackTicks";
static char *synopsis = "Execute a shell command and save the result as a variable.";
static char *desc = ""
"  Backticks(<VARNAME>,<command>)\n\n"
"Be sure to include a full path!\n"

;

static char *do_backticks(const char *command, char *buf, size_t len) 
{
    int fds[2], pid = 0;
    char *ret = NULL;

    memset(buf, 0, len);

    if (pipe(fds)) {    
        ast_log(LOG_WARNING, "Pipe/Exec failed\n");
    } else { /* good to go*/
        
        pid = fork();

        if (pid < 0) { /* ok maybe not */
            ast_log(LOG_WARNING, "Fork failed\n");
            close(fds[0]);
            close(fds[1]);
        } else if (pid) { /* parent */
            close(fds[1]);
            read(fds[0], buf, len);
            close(fds[0]);
            ret = buf;
        } else { /*  child */
            char *argv[255] = {0};
            int argc = 0;
            char *p;
            char *mycmd = ast_strdupa(command);
            
            close(fds[0]);
            dup2(fds[1], STDOUT_FILENO);
            argv[argc++] = mycmd;

            do {
                if ((p = strchr(mycmd, ' '))) {
                    *p = '\0';
                    mycmd = ++p;
                    argv[argc++] = mycmd;
                }
            } while (p);

            close(fds[1]);          
            execv(argv[0], argv); 
            /* DoH! */
            ast_log(LOG_ERROR, "exec of %s failed\n", argv[0]);
            exit(0);
        }
    }

    return buf;
}

static int backticks_exec(struct ast_channel *chan, const char* data)
{
    int res = 0;
    const char *usage = "Usage: Backticks(<VARNAME>,<command>)";
    char buf[1024], *argv[2], *mydata;
    int argc = 0;
    
    
    if (!data) {
        ast_log(LOG_WARNING, "%s\n", usage);
        return -1;
    }

    if (!(mydata = ast_strdupa(data))) {
        ast_log(LOG_ERROR, "Memory Error!\n");
        res = -1;
    } else {
        if((argc = ast_app_separate_args(mydata, ',', argv, sizeof(argv) / sizeof(argv[0]))) < 2) {
            ast_log(LOG_WARNING, "%s\n", usage);
            res = -1;
        }
        
        if (do_backticks(argv[1], buf, sizeof(buf))) {
            pbx_builtin_setvar_helper(chan, argv[0], buf);
        } else {
            ast_log(LOG_WARNING, "No Data!\n");
            res = -1;
        }
    }
    return res;
}

static int backticks_exec_on_manager(struct mansession* s, const struct message* m)
{
    const char *id = astman_get_header(m,"ActionID");
	const char *command = astman_get_header(m,"Command");
	const char *channel = astman_get_header(m,"Channel");
	const char *variable = astman_get_header(m,"Variable");

	struct ast_channel *c = NULL;
	char id_text[256] = "";
	char buf[1024];
	int set_variable = 0;

	if (ast_strlen_zero(command)) {
		astman_send_error(s, m, "Command not specified");
		return 0;
	}
	if (((!ast_strlen_zero(channel))&&(ast_strlen_zero(variable)))||((ast_strlen_zero(channel))&&(!ast_strlen_zero(variable)))) {
		astman_send_error(s, m, "If you set Channel, you have to set Variable and viceversa");
		return 0;
	}
	if ((!ast_strlen_zero(channel))&&(!ast_strlen_zero(variable))) {
		set_variable = 1;
		c = ast_channel_get_by_name(channel);
		if (!c) {
			astman_send_error(s, m, "No such channel");
			return 0;
		}
		ast_channel_lock(c);
	}
	/* Ok, we have everything */
	if (!ast_strlen_zero(id)) {
		snprintf(id_text, sizeof(id_text), "ActionID: %s\r\n", id);
	}
	if (do_backticks(command, buf, sizeof(buf))) {
		astman_append(s, "Response: Success\r\n"
		   "%s"
		   "\r\n\r\n", buf);
		if(set_variable) {
			pbx_builtin_setvar_helper(c, variable, buf);
			ast_channel_unlock(c);
			ast_channel_unref(c);
		}
	} else {
		astman_append(s, "Response: Fail\r\n");
	}
	return 0;

}

static char *function_backticks(struct ast_channel *chan, char *cmd, char *data, char *buf, size_t len)
{
    char *ret = NULL;

    if (do_backticks(data, buf, len)) {
        ret = buf;
    }

    return ret;
}

static struct ast_custom_function backticks_function = {
    .name = "BACKTICKS", 
    .desc = "Executes a shell command and evaluates to the result.", 
    .syntax = "BACKTICKS(<command>)", 
    .synopsis = "Executes a shell command.", 
    .read = function_backticks
};


static int unload_module(void)
{
    ast_custom_function_unregister(&backticks_function);
    ast_manager_unregister(app);
    return ast_unregister_application(app);
}

static int load_module(void)
{
    ast_custom_function_register(&backticks_function);

    int res = ast_register_application(app, backticks_exec, synopsis, desc);
    res |= ast_manager_register2(app, EVENT_FLAG_SYSTEM, backticks_exec_on_manager, desc, synopsis);
    return res;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, "BackTicks Application. Execute shell command and save output into variable",
		.load = load_module,
		.unload = unload_module,
);

