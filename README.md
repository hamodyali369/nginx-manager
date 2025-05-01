```markdown
# Nginx Manager Tool

**Nginx Manager Tool** is a standalone binary utility written in C++ for managing NGINX virtual hosts with a clean interactive interface. It streamlines enabling, disabling, editing, renaming, and removing site configurations — all with auto-reloading and root validation.

## Features

- Enable/disable one or multiple sites at once.
- Rename site configurations (with full handling if enabled).
- Edit files using nano or vim.
- Modify the main `nginx.conf` directly.
- Delete site config files safely.
- Change and persist the NGINX prefix directory (e.g. `/etc/nginx`).
- Auto-detect whether nginx should be reloaded after changes.
- CLI support for quick commands:
  - `-s start|stop|restart` — Manage nginx service
  - `-r` — Reload nginx
  - `-t` — Test nginx configuration
  - `-h`, `--help` — Show help

## Usage

Launch the interactive interface:

```bash
sudo nginx_manager
```

Or run quick commands:

```bash
sudo nginx_manager -r           # Reload nginx
sudo nginx_manager -t           # Test config
sudo nginx_manager -s restart   # Restart nginx
```

## Installation

After downloading the binary:

```bash
sudo mv nginx_manager /usr/local/bin/
sudo chmod +x /usr/local/bin/nginx_manager
```

Now it can be used system-wide as:

```bash
sudo nginx_manager
```

## Notes

- Requires root privileges or `sudo` to run.
- Automatically stores prefix settings and reloads nginx where needed.
```
