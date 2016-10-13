#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "log.h"

static int _log_level = LOG_DEBUG;
static int _log_channels = LOG_CHANNEL_CONSOLE;
static int _log_facility = LOG_LOCAL1;
static int _log_initialized = 0;
static const char *_log_ident = NULL;

static const char *log_default_ident(void)
{
	FILE *self;
	static char line[64];
	char *p = NULL;

	if ((self = fopen("/proc/self/status", "r")) != NULL) {
		while (fgets(line, sizeof(line), self)) {
			if (!strncmp(line, "Name:", 5)) {
				strtok(line, "\t\n");
				p = strtok(NULL, "\t\n");
				break;
			}
		}
		fclose(self);
	}

	return p;
}

static void log_defaults(void)
{
	if (_log_initialized) {
		return;
	}

	if (_log_ident == NULL) {
		_log_ident = log_default_ident();
	}

	if (_log_channels & LOG_CHANNEL_SYSLOG) {
		openlog(_log_ident, 0, _log_facility);
	}

	_log_initialized = 1;
}

void log_output(int level, const char * fmt, ...)
{
	va_list ap;

	if (level == LOG_OFF || level > _log_level) {
		return;
	}

	log_defaults();

	if (_log_channels & LOG_CHANNEL_CONSOLE) {
		fprintf(stdout, "%s: ", _log_ident);
		va_start(ap, fmt);
		vfprintf(stdout, fmt, ap);
		va_end(ap);
	}

	if (_log_channels & LOG_CHANNEL_SYSLOG) {
		va_start(ap, fmt);
		vsyslog(level, fmt, ap);
		va_end(ap);
	}

	if (_log_channels & LOG_CHANNEL_FILE) {
		// TODO
	}

	if (_log_channels & LOG_CHANNEL_REMOTE) {
		// TODO
	}
}

void log_set_level(int level)
{
	_log_level = level;
}

void log_set_channel(int channel)
{
	_log_channels = channel;
	log_close();
	log_defaults();
}

void log_open(const char *ident, int channels)
{
	log_close();
	_log_ident = ident;
	_log_channels = channels;
}

void log_close(void)
{
	if (!_log_initialized) {
		return;
	}

	if (_log_channels & LOG_CHANNEL_SYSLOG) {
		closelog();
	}

	_log_initialized = 0;
}

/* vim: set ts=4 sw=4 tw=0 noexpandtab nolist : */
