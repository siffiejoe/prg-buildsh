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
local ape = require( "ape" )


local function pre_process( argv )
  local ofile = os.tmpname()
  local p = assert( os.getenv( "BUILDSH_PRELOAD" ) )
  assert( ape.env_set( "LD_PRELOAD", p ) )
  assert( ape.env_set( "DYLD_INSERT_LIBRARIES", p ) )
  assert( ape.env_set( "DYLD_FORCE_FLAT_NAMESPACE", "y" ) )
  assert( ape.env_set( "BUILDSH_TEMPFILE", ofile ) )
  return argv, { file=ofile, exec=argv[ 1 ] }
end


local function handle_open( td, pd, pid, _, args )
  local name, dir, fd = args:match( '^%s*([\\x%x]*)%s+(%w+)%s+(%d+)%s*$' )
  if name then
    base.open( td, pd, pid, base.hex2name( name ), dir, fd )
  end
end

local function handle_openat( td, pd, pid, _, args )
  local atfd, name, dir, fd = args:match( '^%s*([%w_]+)%s+([\\x%x]*)%s+(%w+)%s+(%d+)%s*$' )
  if name then
    base.openat( td, pd, pid, atfd, base.hex2name( name ), dir, fd )
  end
end

local function handle_creat( td, pd, pid, _, args )
  local name, fd = args:match( '^%s*([\\x%x]*)%s+(%d+)%s*$' )
  if name then
    base.creat( td, pd, pid, base.hex2name( name ), fd )
  end
end

local function handle_exec( td, pd, pid, _, args )
  local prog = args:match( '^%s*([\\x%x]*)%s*$' )
  if prog then
    base.exec( td, pd, pid, base.hex2name( prog ) )
  end
end

local function handle_fchdir( td, pd, pid, _, args )
  local fd = args:match( "^%s*(%d+)%s*$" )
  if fd then
    base.fchdir( td, pd, pid, fd )
  end
end

local function handle_chdir( td, pd, pid, syscall, args )
  local name = args:match( '^%s*([\\x%x]*)%s*' )
  if name then
    base.chdir( td, pd, pid, base.hex2name( name ) )
  end
end

local function handle_mkdir( td, pd, pid, _, args )
  local name = args:match( '^%s*([\\x%x]*)%s*$' )
  if name then
    base.mkdir( td, pd, pid, base.hex2name( name ) )
  end
end

local function handle_mkdirat( td, pd, pid, _, args )
  local atfd, name = args:match( '^%s*([%w_]+)%s+([\\x%x]*)%s*$' )
  if name then
    base.mkdirat( td, pd, pid, atfd, base.hex2name( name ) )
  end
end

local function handle_fork( td, pd, ppid, _, args )
  local cpid = args:match( '^%s*(%d+)%s*$' )
  if cpid then
    base.fork( td, pd, ppid, cpid )
  end
end

local function handle_rename( td, pd, pid, _, args )
  local from, to = args:match( '^%s*([\\x%x]*)%s+([\\x%x]*)%s*$' )
  if from then
    from, to = base.hex2name( from ), base.hex2name( to )
    base.rename( td, pd, pid, from, to )
  end
end

local function handle_renameat( td, pd, pid, _, args )
  local atfd, from, atfd2, to = args:match( '^%s*([%w_]+)%s+([\\x%x]*)%s+([%w_]+)%s+([\\x%x]*)%s*$' )
  if from then
    from, to = base.hex2name( from ), base.hex2name( to )
    base.renameat( td, pd, pid, atfd, from, atfd2, to )
  end
end


local syscall_re = "^(%d+)%s*([%w_]+)%s+(.*)$"

local syscall_handlers = {
  open = handle_open,
  openat = handle_openat,
  creat = handle_creat,
  exec = handle_exec,
  chdir = handle_chdir,
  fchdir = handle_fchdir,
  mkdir = handle_mkdir,
  mkdirat = handle_mkdirat,
  rename = handle_rename,
  renameat = handle_renameat,
  fork = handle_fork,
}

local function post_process( deps, data, dir )
  local tempdeps = { input = {}, output = {} }
  local pdata = {} -- keep track of cwd and open fds per process
  local first_exec = data.exec
  for line in io.lines( data.file ) do
    local pid, syscall, args = line:match( syscall_re )
    if first_exec then -- we didn't get the first execve (and fork)
      pdata[ pid ] = { cwd = dir }
      base.exec( tempdeps, pdata, pid, first_exec )
      first_exec = nil
    end
    if syscall then
      local sh = syscall_handlers[ syscall ]
      if sh then
        sh( tempdeps, pdata, pid, syscall, args )
      end
    end
  end
  -- remove temp file
  os.remove( data.file )
  -- remove environment variables
  ape.env_delete( "LD_PRELOAD" )
  ape.env_delete( "DYLD_INSERT_LIBRARIES" )
  ape.env_delete( "DYLD_FORCE_FLAT_NAMESPACE" )
  ape.env_delete( "BUILDSH_TEMPFILE" )
  -- avoid rehashing of input files
  base.finish( deps, tempdeps )
end

return {
  name = "LD_PRELOAD",
  pre_process = pre_process,
  post_process = post_process,
}

