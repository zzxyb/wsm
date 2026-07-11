#!/bin/sh

set -eu

home="${DESTDIR:-}$1"
directory="$home/.config/wsm"
target="$directory/outputs.toml"

install -d -m 0755 "$directory"
if [ ! -e "$target" ]; then
	install -m 0644 "$2" "$target"
fi

# A privileged install must still leave the per-user state writable by the
# owner of the home directory. Do not change the contents on upgrades.
if [ "$(id -u)" -eq 0 ]; then
	set -- $(ls -ldn "$home")
	chown "$3:$4" "$directory" "$target"
fi
