--  buildsh -- a portable and flexible build system
--  Copyright (C) 2013  Philipp Janda
--
--  This program is free software: you can redistribute it and/or modify
--  it under the terms of the GNU General Public License as published by
--  the Free Software Foundation, either version 3 of the License, or
--  (at your option) any later version.
--
--  This program is distributed in the hope that it will be useful,
--  but WITHOUT ANY WARRANTY; without even the implied warranty of
--  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--  GNU General Public License for more details.
--
--  You should have received a copy of the GNU General Public License
--  along with this program.  If not, see <http://www.gnu.org/licenses/>.

local base = require( "base" )


local syscalls = "trace=" .. table.concat( {
  "open", "openat", "creat", "execve", "chdir", "fchdir",
  "mkdir", "mkdirat", "rename", "renameat", "clone", "vfork", "fork"
}, "," )

local function pre_process( argv )
  local ofile = os.tmpname()
  local newargv = {
    "strace", "-f", "-xx", "-o", ofile, "-e", "signal=none",
    "-e", syscalls, "--"
  }
  local n = #newargv
  for i = 1,#argv do
    newargv[ n+i ] = argv[ i ]
  end
  return newargv, ofile
end


local function handle_open( td, pd, pid, _, args, fd )
  local name, flags = args:match( '^"([\\x%x]*)",%s*([%u_|]+)' )
  if name then
    local dir = "in"
    if flags:find( "O_WRONLY", 1, true ) or
       flags:find( "O_RDWR", 1, true ) then
      dir = "out"
    end
    base.open( td, pd, pid, base.hex2name( name ), dir, fd )
  end
end

local function handle_openat( td, pd, pid, _, args, fd )
  local atfd, name, flags = args:match( '^([%w_]+),%s*"([\\x%x]*)",%s*([%u_|]+)' )
  if name then
    local dir = "in"
    if flags:find( "O_WRONLY", 1, true ) or
       flags:find( "O_RDWR", 1, true ) then
      dir = "out"
    end
    base.openat( td, pd, pid, atfd, base.hex2name( name ), dir, fd )
  end
end

local function handle_creat( td, pd, pid, _, args, fd )
  local name = args:match( '^"([\\x%x]*)"' )
  if name then
    base.creat( td, pd, pid, base.hex2name( name ), fd )
  end
end

local function handle_exec( td, pd, pid, _, args )
  local prog = args:match( '^"([\\x%x]*)"' )
  if prog then
    base.exec( td, pd, pid, base.hex2name( prog ) )
  end
end

local function handle_fchdir( td, pd, pid, _, args )
  local fd = args:match( "^(%d+)$" )
  if fd then
    base.fchdir( td, pd, pid, fd )
  end
end

local function handle_chdir( td, pd, pid, syscall, args )
  local name = args:match( '^"([\\x%x]*)"' )
  if name then
    base.chdir( td, pd, pid, base.hex2name( name ) )
  end
end

local function handle_mkdir( td, pd, pid, _, args )
  local name = args:match( '^"([\\x%x]*)"' )
  if name then
    base.mkdir( td, pd, pid, base.hex2name( name ) )
  end
end

local function handle_mkdirat( td, pd, pid, _, args )
  local atfd, name = args:match( '^([%w_]+),%s*"([\\x%x]*)"' )
  if name then
    base.mkdirat( td, pd, pid, atfd, base.hex2name( name ) )
  end
end

local function handle_fork( td, pd, ppid, _, _, cpid )
  base.fork( td, pd, ppid, cpid )
end

local function handle_rename( td, pd, pid, _, args )
  local from, to = args:match( '^"([\\x%x]*)",%s*"([\\x%x]*)"' )
  if from then
    from, to = base.hex2name( from ), base.hex2name( to )
    base.rename( td, pd, pid, from, to )
  end
end

local function handle_renameat( td, pd, pid, _, args )
  local atfd, from, atfd2, to = args:match( '^([%w_]+),%s*"([\\x%x]*)",%s*([%w_]+),%s*"([\\x%x]*)"' )
  if from then
    from, to = base.hex2name( from ), base.hex2name( to )
    base.renameat( td, pd, pid, atfd, from, atfd2, to )
  end
end


local function push_unfinished( t, pid, syscall, args )
  local pt = t[ pid ]
  if not pt then
    pt = { [ syscall ] = { args } }
    t[ pid ] = pt
  else
    local st = pt[ syscall ]
    if not st then
      pt[ syscall ] = { args }
    else
      st[ #st+1 ] = args
    end
  end
end

local function pop_unfinished( t, pid, syscall, args, ret )
  local pt = t[ pid ]
  if pt then
    local st = pt[ syscall ]
    if st then
      local a = st[ #st ]
      st[ #st ] = nil
      if a and ret:sub( 1, 1 ) ~= "-" then
        return a .. args
      end
    end
  end
end

local syscall_re = "^(%d+)%s*([%w_]+)%((.*)%)%s*=%s*(%d+)$"
local unfinished_re = "^(%d+)%s*([%w_]+)%(%s*(.*)<unfinished%s*%.%.%.>$"
local resumed_re = "^(%d+)%s*<%.%.%.%s*([%w_]+)%s*resumed>%s*(.*)%)%s*=%s*(%-?%d+)"

local syscall_handlers = {
  open = handle_open,
  openat = handle_openat,
  creat = handle_creat,
  execve = handle_exec,
  chdir = handle_chdir,
  fchdir = handle_fchdir,
  mkdir = handle_mkdir,
  mkdirat = handle_mkdirat,
  rename = handle_rename,
  renameat = handle_renameat,
  clone = handle_fork,
  vfork = handle_fork,
  fork = handle_fork,
}

local function post_process( deps, data, dir )
  local tempdeps = { input = {}, output = {} }
  local pdata = {} -- keep track of cwd and open fds per process
  local unfinished = {}
  local first = true
  for line in io.lines( data ) do
    local pid, syscall, args, ret = line:match( syscall_re )
    if not pid then
      pid, syscall, args = line:match( unfinished_re )
      if syscall then
        push_unfinished( unfinished, pid, syscall, args )
        syscall = nil -- don't call handler yet below ...
      end
      if not pid then
        pid, syscall, args, ret = line:match( resumed_re )
        if syscall then
          args = pop_unfinished( unfinished, pid, syscall, args, ret )
          if not args then
            syscall = nil -- don't call handler yet below ...
          end
        end
      end
    end
    if syscall then
      if first then -- fake a fork, because strace starts at execve
        pdata[ pid ] = { cwd = dir }
        first = false
      end
      local sh = syscall_handlers[ syscall ]
      if sh then
        sh( tempdeps, pdata, pid, syscall, args, ret )
      end
    end
  end
  -- remove temp file
  os.remove( data )
  base.finish( deps, tempdeps )
end

return {
  name = "strace",
  pre_process = pre_process,
  post_process = post_process,
}

