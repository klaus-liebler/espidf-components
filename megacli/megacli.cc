#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include "megacli.hh"
#include <vector>
#include <argtable3/argtable3.h>
#include "esp_system.h"
#include "esp_vfs_dev.h"
#include "esp_log.h"

#define TAG "CLI"

namespace CLI
{
	int Static_writefn(void *data, const char *buffer, int size)
	{
		IShellCallback *cb = static_cast<IShellCallback *>(data);
		cb->printf("%.*s", size, buffer);
		return 0;
	}

	
}