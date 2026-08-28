<p align="center">
  <img src="https://img.shields.io/badge/language-C%2B%2B17-blue.svg" alt="C++17">
  <img src="https://img.shields.io/badge/platform-Linux-important.svg" alt="Linux">
  <img src="https://img.shields.io/badge/license-MIT-green.svg" alt="MIT">
  <img src="https://img.shields.io/badge/requires-root-red.svg" alt="Root required">
</p>

# NGINX Manager

A standalone C++ utility for managing **NGINX virtual hosts** from a clean, interactive terminal interface. Enable, disable, rename, edit, and delete site configurations — with automatic reloads, config backups, and root validation.

No dependencies at runtime — just a single static-free binary.

---

## ✨ Features

- **Enable/Disable sites** — toggle one or multiple sites at once via symlinks in `sites-enabled`.
- **Rename site configs** — with full handling of the `sites-enabled` symlink when the site is active.
- **Edit configs comfortably** — `nano` or `vim`, with optional automatic backups.
- **Edit `nginx.conf` directly** — the main configuration file, with backup support.
- **Delete site configs safely** — per-file confirmation and multi-delete support.
- **Backups** — list, restore, and delete backups stored in `/etc/nginx-manager/backups`.
- **Nginx quick commands** — start, stop, restart, reload, and test (`nginx -t`) from the menu.
- **Configurable NGINX prefix** — change the nginx directory (e.g. `/etc/nginx`) and persist it.
- **Auto-reload** — smart detection of whether nginx needs a reload after changes.
- **Persistent preferences** — backup-on-edit, default editor, and exit-after-action settings are saved.

---

## 📦 Requirements

- Linux (any distro)
- `g++` (only for building from source)
- `nginx` installed (for the tool to manage)
- `nano` or `vim` (for editing configs)
- Root privileges — the tool validates that it runs as root

---

## 🚀 Installation

### Option 1 — Direct download from Releases

Grab the latest prebuilt binary from the [Releases page](../../releases/latest):

```bash
# Download the binary
wget -O nginx_manager https://github.com/digitaltrekkerr/nginx-manager/releases/latest/download/nginx_manager

# Install it system-wide
sudo mv nginx_manager /usr/local/bin/
sudo chmod +x /usr/local/bin/nginx_manager
```

Now run it anywhere:

```bash
sudo nginx_manager
```

### Option 2 — Build from source

```bash
# Clone the repository
git clone https://github.com/digitaltrekkerr/nginx-manager.git
cd nginx-manager

# Build
g++ -std=c++17 -O2 -s -o nginx_manager nginx_manager.cpp

# Install
sudo mv nginx_manager /usr/local/bin/
sudo chmod +x /usr/local/bin/nginx_manager
```

> 💡 `-s` strips the binary to keep it small. Remove it if you need debug symbols.

---

## 🖥️ Usage

### Interactive mode

Run with no arguments to get the interactive menu:

```bash
sudo nginx_manager
```

```
--- NGINX Manager (Prefix: /etc/nginx) ---
1. Enable/Disable sites
2. Rename a site config
3. Edit a site config
4. Delete a site config
5. Edit main nginx.conf
7. Restore from backup
8. Delete backup
9. Nginx status/test/control (quick commands)
0. Exit
```

### CLI mode

Quick commands for scripting and one-off tasks:

| Command | Description |
|---------|-------------|
| `sudo nginx_manager -r` | Reload nginx (`nginx -s reload`) |
| `sudo nginx_manager -t` | Test nginx configuration (`nginx -t`) |
| `sudo nginx_manager -s start` | Start nginx service (`systemctl start nginx`) |
| `sudo nginx_manager -s stop` | Stop nginx service (`systemctl stop nginx`) |
| `sudo nginx_manager -s restart` | Restart nginx service (`systemctl restart nginx`) |
| `sudo nginx_manager -h` / `--help` | Show help |

### Configuration preferences

These are persisted in `/etc/nginx-manager/nginx-manager.conf`:

| Command | Description |
|---------|-------------|
| `sudo nginx_manager --prefix /path/to/nginx` | Change the nginx prefix (must contain `nginx.conf`, `sites-available/`, `sites-enabled/`) |
| `sudo nginx_manager -b yes\|no` | Always / never back up configs before editing |
| `sudo nginx_manager --editor nano\|vim` | Set the default text editor |
| `sudo nginx_manager -e yes\|no` | Exit immediately after an action in interactive mode |

---

## 🗂️ How it works

```
/etc/nginx/
├── sites-available/   # all site configs live here
├── sites-enabled/     # symlinks to active sites
└── nginx.conf         # main configuration

/etc/nginx-manager/
├── nginx-manager.conf # saved preferences
└── backups/           # config backups (site_*.conf and nginx.conf)
```

The tool operates on `sites-available` and toggles sites by creating/removing symlinks in `sites-enabled` — the standard Debian/Ubuntu nginx layout. After any change it reloads nginx automatically.

---

## 🔧 Building yourself

```bash
g++ -std=c++17 -O2 -s -o nginx_manager nginx_manager.cpp
```

Requirements: `g++` with C++17 support (`std::filesystem`), Linux. No third-party libraries.

---

## 📄 License

MIT — free to use, modify, and distribute.

---

## 🤝 Contributing

Found a bug or want a new feature? Open an [issue](../../issues) or submit a [pull request](../../pulls).