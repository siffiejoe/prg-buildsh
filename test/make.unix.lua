#!/usr/bin/env luamake

local cflags = make.qw[[
  -Wall -Wextra -Wfatal-errors -fno-common -Os
]]

local function build()
  $gcc( cflags, "-o", "unixsys", "unixsys.c" )
  make.run"./unixsys"()
end

return {
  build = build,
}

