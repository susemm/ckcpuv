#ifndef __LOG_H
#define __LOG_H

#define ASSERT(expr, msg)   if (expr) {} else { log_err(msg); return; }
#define ASSERT_RET(expr, msg, ret)   if (expr) {} else { log_err(msg); return (ret); }
#define ASSERT_BREAK(expr, msg)   if (expr) {} else { log_err(msg); break; }

#define SHOW_FUNC_ENTER() log_info("++++ %s enter", __FUNCTION__)
#define SHOW_FUNC_EXIT()  log_info("---- %s exit", __FUNCTION__)

int log_info(const char* format, ...);
int log_err(const char* format, ...);

#endif

