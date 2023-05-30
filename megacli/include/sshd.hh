#pragma once
#include <libssh/libssh.h>
#include <libssh/server.h>
#include <libssh/callbacks.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <sys/queue.h>
#include <array>
#include <vector>
#include "shell_handler.hh"

namespace sshd
{
	class User{
		public:
		const char* Username;
		const char* Password;
		bool IsPrivileged;
		User(const char* Username, const char* Password, bool IsPrivileged):Username(Username), Password(Password), IsPrivileged(IsPrivileged){}
	};
	
	class ConnectionCtx : public IShellCallback
	{
	public:
		ssh_session cc_session{nullptr};
		ssh_channel cc_channel{nullptr};
		bool cc_didauth{false};
		bool cc_didpty{false};
		bool cc_didshell{false};
		ShellId shellId;
		int cc_cols{0};
		int cc_rows{0};
		int cc_py{0};
		int cc_px{0};
		char cc_term[16];
		const User* user;
		
		const char* GetUsername() override{
			if(!user || !user->Username) return "????";
			return user->Username;
		}
		ConnectionCtx(ssh_session session) : cc_session(session)
		{
		}

		~ConnectionCtx()
		{
		}

		bool IsPrivilegedUser() override{
			if(!user) return false;
			return user->IsPrivileged;
		}

		int printChar(char c) override
		{
			return ssh_channel_write(cc_channel, &c, 1);
			
		}
		int printf(const char *fmt, ...) override
		{
			char tmp[512];
			va_list args;
			va_start(args, fmt);
			int size = vsnprintf(tmp, sizeof(tmp), fmt, args);
			va_end(args);
			return ssh_channel_write(cc_channel, &tmp, size);

		}
	};



	class SshDemon
	{
	private:
		ssh_event sshevent;
		ssh_bind sshbind;
		struct ssh_server_callbacks_struct server_cb
		{
		};
		struct ssh_callbacks_struct generic_cb
		{
		};
		struct ssh_bind_callbacks_struct bind_cb
		{
		};
		struct ssh_channel_callbacks_struct channel_cb
		{
		};
		int auth_methods;
		const char *host_key;
		IShellHandler *shellHandler;
		std::array<ConnectionCtx *, 3> connection_ctx_array{nullptr, nullptr, nullptr};

		ConnectionCtx *lookup_connectioncontext(ssh_session session);
		static int data_function(ssh_session session, ssh_channel channel, void *data, uint32_t len, int is_stderr, void *userdata);
		static int pty_request(ssh_session session, ssh_channel channel, const char *term, int cols, int rows, int py, int px, void *userdata);
		static int shell_request(ssh_session session, ssh_channel channel, void *userdata);
		static int exec_request(ssh_session session, ssh_channel channel, const char *command, void *userdata);
		static int pty_resize(ssh_session session, ssh_channel channel, int cols, int rows, int py, int px, void *userdata);
		static ssh_channel channel_open(ssh_session session, void *userdata);
		static void incoming_connection(ssh_bind sshbind, void *userdata);
		static int auth_password(ssh_session session, const char *user, const char *password, void *userdata);
		int create_new_server();
		void dead_eater();
		void task();
		static void static_task(void *arg);
		std::vector<User>* users;
	public:
		static SshDemon *InitAndRunSshD(const char *hardcoded_example_host_key, IShellHandler *handler, std::vector<User>* users);
	};

}