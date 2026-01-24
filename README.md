# confq

A simple scriptable command-line utility for querying and editing linux configuration files (`.conf`)

## Usage
confq [FILE] <operation> [options...]

Operations:\n"

-Qs [section]: Query if section exists

-Qv [section] [value]: Query if value exists

-Qk [section] [key]: Query if key exists

-Rs [section]: Remove entire section

-Rv [section] [value]: Remove value if exists

-Rk [section] [key]: Remove value with specified key if it exists

-Sv [section] [value]: Set value, does nothing if exists

-Sv [section] [key] [value]: Set key to value, overriding if exists

## Example Usage:
```
confq /etc/pacman.conf -Qv "[options]" "CheckSpace" > /etc/pacman.conf.new

confq /etc/pacman.conf -Qk "[options]" "HoldPkg" > /etc/pacman.conf.new

confq /etc/pacman.conf -Rs "[multilib]" > /etc/pacman.conf.new

confq /etc/pacman.conf -Rv "[options]" "CheckSpace" > /etc/pacman.conf.new

confq /etc/pacman.conf -Rk "[options]" "HoldPkg" > /etc/pacman.conf.new

confq /etc/pacman.conf -Sv "[options]" "ILoveCandy" > /etc/pacman.conf.new

confq /etc/pacman.conf -Sk "[multilib]" "Include" "/etc/pacman.d/mirrorlist" > /etc/pacman.conf.new
```
