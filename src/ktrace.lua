local base = require( "base" )
local ape = require( "ape" )


local function pre_process( argv )
  local ofile = os.tmpname()
  local newargv = {
    "ktrace", "-f", ofile, "-i", "-tcn"
  }
  local n = #newargv
  for i = 1,#argv do
    newargv[ n+i ] = argv[ i ]
  end
  return newargv, ofile
end


-- handler for debugging
local function print_syscall( td, pd, pid, syscall, args, ret, ... )
  print( pid, ret .. " == ", syscall, ... )
end


local function handle_open( td, pd, pid, _, args, fd, name )
  if name then
    local dir = "in"
    if args:find( "O_WRONLY", 1, true ) or
       args:find( "O_RDWR", 1, true ) then
      dir = "out"
    end
    base.open( td, pd, pid, name, dir, tonumber( fd ) )
  end
end

local function handle_openat( td, pd, pid, _, args, fd, name )
  local atfd = args:match( "^%(0x(%x+)," )
  if atfd then
    atfd = tonumber( atfd, 16 )
  end
  if name and atfd then
    local dir = "in"
    local oflags = args:match( "^%(0x%x+,0x%x+,0x(%x+)," )
    if oflags then
      oflags = tonumber( oflags, 16 )
      if oflags and ape.hacks.O_ACCMODE then
        oflags = oflags % (ape.hacks.O_ACCMODE+1)
        if oflags == ape.hacks.O_WRONLY or
           oflags == ape.hacks.O_RDWR then
          dir = "out"
        end
      end
    else
      if args:find( "O_WRONLY", 1, true ) or
         args:find( "O_RDWR", 1, true ) then
        dir = "out"
      end
    end
    base.openat( td, pd, pid, atfd, name, dir, tonumber( fd ) )
  end
end

local function handle_exec( td, pd, pid, _, _, _, prog )
  if prog then
    base.exec( td, pd, pid, prog )
  end
end

local function handle_fchdir( td, pd, pid, _, args )
  local fd = args:match( "^%(0x(%x+)%)" )
  if fd then
    base.fchdir( td, pd, pid, tonumber( fd, 16 ) )
  end
end

local function handle_chdir( td, pd, pid, _, _, _, name )
  if name then
    base.chdir( td, pd, pid, name )
  end
end

local function handle_mkdir( td, pd, pid, _, _, _, name )
  if name then
    base.mkdir( td, pd, pid, name )
  end
end

local function handle_mkdirat( td, pd, pid, _, args, _, name )
  local atfd = args:match( "^%(0x(%x+)," )
  if atfd then
    atfd = tonumber( atfd, 16 )
  end
  if name and atfd then
    base.mkdirat( td, pd, pid, atfd, name )
  end
end

local function handle_fork( td, pd, ppid, _, _, cpid )
  base.fork( td, pd, ppid, cpid )
end

local function handle_rename( td, pd, pid, _, _, _, from, to )
  if from and to then
    base.rename( td, pd, pid, from, to )
  end
end

local function handle_renameat( td, pd, pid, _, args, _, from, to )
  local atfd, atfd2 = args:match( "^%(0x(%x+),0x%x+,0x(%x+)," )
  if atfd then
    atfd = tonumber( atfd, 16 )
    atfd2 = tonumber( atfd2, 16 )
  end
  if from and to and atfd and atfd2 then
    base.renameat( td, pd, pid, atfd, from, atfd2, to )
  end
end


local function push_syscall( t, pid, syscall, args )
  local pt = t[ pid ]
  if not pt then
    pt = { { [ syscall ] = args } }
    t[ pid ] = pt
  else
    pt[ #pt+1 ] = { [ syscall ] = args }
  end
end

local function set_syscall_arg( t, pid, s )
  local pt = t[ pid ]
  if pt then
    local st = pt[ #pt ]
    if st then
      st[ #st+1 ] = s
    end
  end
end

local function pop_syscall( t, pid, syscall, ret )
  local pt = t[ pid ]
  if pt then
    local index, value
    for i = #pt, 1, -1 do
      local st = pt[ i ]
      if st[ syscall ] then
        index, value = i, st
        break
      end
    end
    if index then
      for i = index+1, #pt do
        pt[ i-1 ] = pt[ i ]
      end
      pt[ #pt ] = nil
    end
    if ret:sub( 1, 1 ) ~= '-' then
      return value
    end
  end
end

local syscall_re = "^%s*(%d+).+CALL%s*([%w_]+)(.-)$"
local nami_re = "^%s*(%d+).+NAMI%s*\"(.-)\"%s*$"
local ret_re = "^%s*(%d+).+RET%s*([%w_]+)%s*(%-?%d+).-$"

local syscall_handlers = {
  open = handle_open,
  openat = handle_openat,
  execve = handle_exec,
  chdir = handle_chdir,
  fchdir = handle_fchdir,
  mkdir = handle_mkdir,
  mkdirat = handle_mkdirat,
  rename = handle_rename,
  renameat = handle_renameat,
  vfork = handle_fork,
  fork = handle_fork,
  pdfork = handle_fork,
  rfork = handle_fork,
}

local function post_process( deps, data, dir )
  local tempdeps = { input = {}, output = {} }
  local pdata = {} -- keep track of cwd and open fds per process
  local first = true
  local kdump = os.tmpname()
  os.execute( "kdump -s -f \""..data.."\" > \""..kdump.."\"" )
  os.remove( data )
  local stack = {}
  for line in io.lines( kdump ) do
    local pid, syscall, args, ret = line:match( syscall_re )
    if pid then
      push_syscall( stack, pid, syscall, args )
    else
      pid, syscall, ret = line:match( ret_re )
      if pid then
        local sdata = pop_syscall( stack, pid, syscall, ret )
        local sh = syscall_handlers[ syscall ]
        if first then -- fake a fork
          pdata[ pid ] = { cwd = dir }
          first = false
        end
        if sh and sdata then
          sh( tempdeps, pdata, pid, syscall, sdata[ syscall ], ret,
              unpack( sdata ) )
        end
      else
        pid, args = line:match( nami_re )
        if pid then
          set_syscall_arg( stack, pid, args )
        end
      end
    end
  end
  -- remove temp file
  os.remove( kdump )
  base.finish( deps, tempdeps )
end

return {
  name = "ktrace/kdump",
  pre_process = pre_process,
  post_process = post_process,
}

