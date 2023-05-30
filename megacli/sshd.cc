#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include "config.h"
#include <libssh/callbacks.h>
#include <libssh/server.h>
#include <poll.h>
#include <sys/queue.h>
#include "sshd.hh"
#include "esp_log.h"
#include <esp_vfs.h>
#define TAG "SSHD"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <array>



extern "C"{
	struct ssh_poll_handle_struct *ssh_bind_get_poll(struct ssh_bind_struct *);
	int ssh_event_add_poll(ssh_event event, struct ssh_poll_handle_struct *);
	int ssh_event_remove_poll(ssh_event event, struct ssh_poll_handle_struct *);
}

namespace sshd{
	constexpr const char *KEYS_FOLDER{"/ssh"};
	static size_t sshfs_hostkey_offset = 0;
	static ssize_t sshfs_read(void *ctx, int fd, void *dst, size_t size)
	{
		if (fd == 10)
		{
			//ESP_LOGI(TAG, "sshfs_read for fd=%i with size=%d called. Current offset=%d", fd, size, sshfs_hostkey_offset);
			const char* hostkey=(const char *)ctx;
			memcpy(dst, hostkey + sshfs_hostkey_offset, size);
			sshfs_hostkey_offset += size;
			return size;
		}
		return -1;
	}
	static int sshfs_open(void *ctx, const char *path, int flags, int mode)
	{
		if (!strcmp("/hostkey", path))
		{
			ESP_LOGI(TAG, "vfs_open for hostkey called");
			sshfs_hostkey_offset = 0;
			return 10;
		}
		ESP_LOGW(TAG, "vfs_open for unknown file %s called", path);
		return -1;
	}
	static int sshfs_close(void *ctx, int fd)
	{
		ESP_LOGI(TAG, "vfs_close called for fd=%i", fd);
		sshfs_hostkey_offset = 0;
		return 0;
	}
	static int sshfs_fstat(void *ctx, int fd, struct stat *st)
	{
		if (fd == 10)
		{
			size_t len = strlen((const char *)ctx);
			st->st_size = len;
			return 0;
		}
		ESP_LOGI(TAG, "vfs_fstat, called for fd=%i", fd);
		return -1;
	}
	void sshd_prepare_vfs(const char *base64_key)
	{
		esp_vfs_t myfs = {};
		myfs.flags = ESP_VFS_FLAG_CONTEXT_PTR;
		myfs.write_p = NULL;
		myfs.open_p = &sshfs_open;
		myfs.fstat_p = &sshfs_fstat;
		myfs.close_p = &sshfs_close;
		myfs.read_p = &sshfs_read;
		ESP_ERROR_CHECK(esp_vfs_register("/ssh", &myfs, (void*)base64_key));
	}

	ConnectionCtx *
	SshDemon::lookup_connectioncontext(ssh_session session)
	{
		for (auto ret : this->connection_ctx_array) {
  			if (ret->cc_session == session)
				return ret;
		}
		return nullptr;
	}

	int	SshDemon::data_function(ssh_session session, ssh_channel channel, void *data, uint32_t len, int is_stderr, void *userdata)
	{
		//ESP_LOGI(TAG, "data_function! with data len=%lu", len);
		SshDemon *sshd = static_cast<SshDemon*>(userdata);
		ConnectionCtx* cc = sshd->lookup_connectioncontext(session);
		int i;
		char c;
		for (i = 0; i < len; i++)
		{
			c = ((char *)data)[i];
			if (c == 0x4) /* ^D */
			{
				ssh_channel_send_eof(channel);
				ssh_channel_close(channel);
				return len;
			}
		}
		sshd->shellHandler->handleChars(static_cast<const char*>(data), len, cc->shellId, cc);
		return len;
	}

	int SshDemon::exec_request(ssh_session session, ssh_channel channel, const char *command, void *userdata)
	{
		ESP_LOGD(TAG, "exec_request!");
		SshDemon *sshd = static_cast<SshDemon*>(userdata);
		ConnectionCtx* cc = sshd->lookup_connectioncontext(session);
		if (cc->cc_didshell)
			return SSH_ERROR;
		sshd->shellHandler->handleChars(command, strlen(command), cc->shellId, cc);
		ssh_channel_send_eof(channel);
		ssh_channel_close(channel);
		return SSH_OK;
	}

	int SshDemon::pty_request(ssh_session session, ssh_channel channel, const char *term, int cols, int rows, int py, int px, void *userdata)
	{
		ESP_LOGD(TAG, "pty_request!");
		SshDemon *sshd = static_cast<SshDemon*>(userdata);
		ConnectionCtx* cc = sshd->lookup_connectioncontext(session);

		if (cc->cc_didpty)
			return SSH_ERROR;
		cc->cc_cols = cols;
		cc->cc_rows = rows;
		cc->cc_px = px;
		cc->cc_py = py;
		strlcpy(cc->cc_term, term, sizeof(cc->cc_term));
		cc->cc_didpty = true;
		return SSH_OK;
	}

	int SshDemon::shell_request(ssh_session session, ssh_channel channel, void *userdata)
	{
		ESP_LOGD(TAG, "shell_request!");
		SshDemon *sshd = static_cast<SshDemon*>(userdata);
		ConnectionCtx* cc = sshd->lookup_connectioncontext(session);
		if (cc->cc_didshell)
			return SSH_ERROR;
		cc->cc_didshell = true;
		cc->shellId=sshd->shellHandler->beginShell(cc);
		return SSH_OK;
	}

	int SshDemon::pty_resize(ssh_session session, ssh_channel channel, int cols, int rows, int py, int px, void *userdata)
	{
		ESP_LOGD(TAG, "pty_resize!");
		SshDemon *sshd = static_cast<SshDemon*>(userdata);
		ConnectionCtx* cc = sshd->lookup_connectioncontext(session);
		cc->cc_cols = cols;
		cc->cc_rows = rows;
		cc->cc_px = px;
		cc->cc_py = py;
		return SSH_OK;
	}

	ssh_channel SshDemon::channel_open(ssh_session session, void *userdata)
	{
		ESP_LOGD(TAG, "channel_open!");
		SshDemon *sshd = static_cast<SshDemon*>(userdata);
		ConnectionCtx* cc = sshd->lookup_connectioncontext(session);
		if (cc == nullptr){
			ESP_LOGD(TAG, "cc == nullptr in " __FILE__);
			return nullptr;
		}
		if (cc->cc_channel!=nullptr){
			ESP_LOGD(TAG, "cc->cc_channel!=nullptr in " __FILE__);
			return nullptr;
		}
		ESP_LOGD(TAG, "create a new channel and store in ConnectionCtx and register callbacks (already prepared in demon!");	
		ssh_channel channel = ssh_channel_new(session);
		cc->cc_channel=channel;
		ssh_set_channel_callbacks(cc->cc_channel, &sshd->channel_cb);
		return cc->cc_channel;
	}

	void SshDemon::incoming_connection(ssh_bind sshbind, void *userdata)
	{
		ESP_LOGD(TAG, "incoming_connection!");
		SshDemon *sshd = static_cast<SshDemon*>(userdata);
		long t = 0;
		ConnectionCtx *cc{nullptr};
		for(size_t i=0;i<sshd->connection_ctx_array.size();i++){
			if(sshd->connection_ctx_array[i]==nullptr){
				ESP_LOGI(TAG, "Creating new connection ctx with session in slot %d", i);
				cc = new ConnectionCtx(ssh_new());
				sshd->connection_ctx_array[i]=cc;
				break;
			}
		}
		if(cc==nullptr){
			ESP_LOGW(TAG, "Did not found a free session slot");
			return;
		}
		assert(cc->cc_session!=nullptr);
		
		if (ssh_bind_accept(sshbind, cc->cc_session) == SSH_ERROR)
		{
			goto cleanup;
		}
		ESP_LOGD(TAG, "Set various session stuff");
		ssh_set_callbacks(cc->cc_session, &(sshd->generic_cb));
		ssh_set_server_callbacks(cc->cc_session, &sshd->server_cb);
		ssh_set_auth_methods(cc->cc_session, sshd->auth_methods);
		ssh_set_blocking(cc->cc_session, 0);
		(void)ssh_options_set(cc->cc_session, SSH_OPTIONS_TIMEOUT, &t);
		(void)ssh_options_set(cc->cc_session, SSH_OPTIONS_TIMEOUT_USEC, &t);
		ESP_LOGD(TAG, "Handle kex");
		if (ssh_handle_key_exchange(cc->cc_session) == SSH_ERROR)
		{
			ESP_LOGW(TAG, "Kex produced an error");
			ssh_disconnect(cc->cc_session);
			goto cleanup;
		}
		/*
		 * Since we set the socket to non-blocking already,
		 * ssh_handle_key_exchange will return SSH_AGAIN.
		 * Start polling the socket and let the main loop drive the kex.
		 */
		ESP_LOGD(TAG, "Add the newly generated session to event handling");
		ssh_event_add_session(sshd->sshevent, cc->cc_session);
		return;
	cleanup:
		ssh_free(cc->cc_session);
		delete cc;
	}

	int SshDemon::auth_password(ssh_session session, const char *username, const char *password, void *userdata)
	{
		SshDemon *sshd = static_cast<SshDemon*>(userdata);
		ConnectionCtx* cc = sshd->lookup_connectioncontext(session);
		if (cc == NULL)
			return SSH_AUTH_DENIED;
		if (cc->cc_didauth)
			return SSH_AUTH_DENIED;
		for(size_t i=0;i<sshd->users->size();i++){
			User* user = &sshd->users->at(i);
			if(strcmp(username, user->Username) == 0 && strcmp(password, user->Password) == 0){
				cc->cc_didauth = true;
				cc->user=user;
				assert(cc->user);
				assert(cc->user->Username);
				assert(cc->user->Password);
				ESP_LOGI(TAG, "SSH_AUTH_SUCCESS!");
				return SSH_AUTH_SUCCESS;
			}
		}
		ESP_LOGW(TAG, "SSH_AUTH_DENIED!");
		return SSH_AUTH_DENIED;
	}

	int	SshDemon::create_new_server()
	{
		this->server_cb.auth_password_function=SshDemon::auth_password;
		this->server_cb.channel_open_request_session_function=SshDemon::channel_open;
		this->server_cb.userdata=this;
		ssh_callbacks_init(&this->server_cb);//setzt die "size" der Struktur
		
		this->generic_cb.userdata=this;
		ssh_callbacks_init(&this->generic_cb);
		
		this->bind_cb.incoming_connection=SshDemon::incoming_connection;
		ssh_callbacks_init(&this->bind_cb);

		this->channel_cb.channel_data_function=SshDemon::data_function;
		this->channel_cb.channel_exec_request_function=SshDemon::exec_request;
		this->channel_cb.channel_pty_request_function=SshDemon::pty_request;
		this->channel_cb.channel_pty_window_change_function=SshDemon::pty_resize;
		this->channel_cb.channel_shell_request_function=SshDemon::shell_request;
		this->channel_cb.userdata=this;
		ssh_callbacks_init(&this->channel_cb);


		sshd_prepare_vfs(this->host_key);

		ESP_LOGD(TAG, "Calling ssh_bind_new()");
		this->sshbind = ssh_bind_new();
		if (sshbind == nullptr)
		{
			ESP_LOGE(TAG, "sshbind == NULL");
			return SSH_ERROR;
		}
		int error = 0;
		ESP_LOGD(TAG, "Setting bind options");
		error = ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_LOG_VERBOSITY_STR, "1");
		if (error != 0)
		{
			ESP_LOGE(TAG, "ssh_bind_options_set SSH_BIND_OPTIONS_LOG_VERBOSITY_STR returned=%i", error);
			return SSH_ERROR;
		}
		error = ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_HOSTKEY, "/ssh/hostkey");
		if (error != 0)
		{
			ESP_LOGE(TAG, "ssh_bind_options_set SSH_BIND_OPTIONS_HOSTKEY returned=%i", error);
			return SSH_ERROR;
		}
		error = ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDPORT_STR, "22");
		if (error != 0)
		{
			ESP_LOGE(TAG, "ssh_bind_options_set SSH_BIND_OPTIONS_BINDPORT_STR returned=%i", error);
			return SSH_ERROR;
		}
		error = ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDADDR, "0.0.0.0");
		if (error != 0)
		{
			ESP_LOGE(TAG, "ssh_bind_options_set SSH_BIND_OPTIONS_BINDADDR returned=%i", error);
			return SSH_ERROR;
		}

		ESP_LOGD(TAG, "Setting callbacks");
		error = ssh_bind_set_callbacks(sshbind, &bind_cb, this);
		if (error != 0)
		{
			ESP_LOGE(TAG, "ssh_bind_set_callbacks returned=%i", error);
			return SSH_ERROR;
		}
		ESP_LOGD(TAG, "Calling ssh_bind_listen()");
		error = ssh_bind_listen(sshbind);
		if (error != 0)
		{
			ESP_LOGE(TAG, "ssh_bind_listen returned=%i", error);
			ssh_bind_free(sshbind);
			return SSH_ERROR;
		}
		ESP_LOGD(TAG, "Calling ssh_bind_set_blocking()");
		ssh_bind_set_blocking(sshbind, 0);

		ESP_LOGD(TAG, "Calling ssh_event_add_poll()");
		
		error = ssh_event_add_poll(sshevent, ssh_bind_get_poll(sshbind));
		if (error != 0)
		{
			ESP_LOGE(TAG, "ssh_event_add_poll returned=%i", error);
			return SSH_ERROR;
		}
		return SSH_OK;
	}

	void SshDemon::dead_eater()
	{
		int status;
		for (int i=0;i<this->connection_ctx_array.size();i++) {
			ConnectionCtx* cc=this->connection_ctx_array.at(i);
			if(cc==nullptr) continue;
			assert(cc->cc_session!=nullptr);
			status = ssh_get_status(cc->cc_session);
			if (status & (SSH_CLOSED | SSH_CLOSED_ERROR))
			{
				if(cc->cc_didshell){
					this->shellHandler->endShell(cc->shellId, cc);
				}
				if (cc->cc_channel)
				{
					ssh_channel_free(cc->cc_channel);
					ssh_remove_channel_callbacks(cc->cc_channel, &channel_cb);
				}
				ssh_event_remove_session(sshevent, cc->cc_session);
				ssh_free(cc->cc_session);
				delete cc;
				this->connection_ctx_array.at(i)=nullptr;
			}
		}
	}

	void SshDemon::task()
	{

		this->auth_methods = SSH_AUTH_METHOD_PASSWORD;
		if (ssh_init() != SSH_OK)
		{
			ESP_LOGE(TAG, "ssh_init()!=SSH_OK");
			return;
		}
		ssh_event event = ssh_event_new();
		if (!event)
		{
			ESP_LOGE(TAG, "ssh_event_new()");
			return;
		}
		this->sshevent = event;
		if (create_new_server() != SSH_OK)
		{
			ESP_LOGE(TAG, "create_new_server(sc)");
			return;
		}
		bool time_to_die = false;
		while (!time_to_die)
		{
			int error = ssh_event_dopoll(sshevent, 1000);
			if (error == SSH_ERROR || error == SSH_AGAIN)
			{
				/* check if any clients are dead and consume 'em */
				dead_eater();
			}
		}
		//terminate_server(sc);
		ssh_event_free(event);
		ssh_finalize();
	}

	void SshDemon::static_task(void *arg)
	{
		SshDemon* myself =static_cast<SshDemon*>(arg);
		myself->task();
	}

	SshDemon *SshDemon::InitAndRunSshD(const char *host_key, IShellHandler* handler, std::vector<User>* users)
	{
		SshDemon* demon = new SshDemon();
		demon->host_key=host_key;
		demon->shellHandler=handler;
		demon->users=users;
		xTaskCreate(SshDemon::static_task, "sshd", 4096*4, (void*)demon, 10, nullptr);
		return demon;
	}
}

