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

Note: DO NOT use the `>` operator to write to the same file, e.g `confq /etc/pacman.conf -Sv "[options]" "ILoveCandy" > /etc/pacman.conf`, 
bash will delete the original file before `confq` is able to read it! If you want to write to the same file you are reading from without 
making a temporary file, the `sponge` command (from `moreutils`)

```
confq /etc/pacman.conf -Qv "[options]" "CheckSpace" | sudo sponge /etc/pacman.conf

confq /etc/pacman.conf -Qk "[options]" "HoldPkg" | sudo sponge /etc/pacman.conf

confq /etc/pacman.conf -Rs "[multilib]" | sudo sponge /etc/pacman.conf

confq /etc/pacman.conf -Rv "[options]" "CheckSpace" | sudo sponge /etc/pacman.conf

confq /etc/pacman.conf -Rk "[options]" "HoldPkg" | sudo sponge /etc/pacman.conf

confq /etc/pacman.conf -Sv "[options]" "ILoveCandy" | sudo sponge /etc/pacman.conf

confq /etc/pacman.conf -Sk "[multilib]" "Include" "/etc/pacman.d/mirrorlist" | sudo sponge /etc/pacman.conf
```
