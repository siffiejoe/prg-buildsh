local ape = require( "ape" )

local _M = {}


local function hex2char( s )
  return string.char( tonumber( s, 16 ) )
end

function _M.hex2name( s )
  return s:gsub( "\\x(%x%x)", hex2char )
end


local get_path
do
  local cwd = ape.filepath_get( ape.FILEPATH_NATIVE )
  local cwd_up = cwd and cwd:upper()
  local dirsep = package.config:sub( 1, 1 )

  function get_path( ccwd, p, normalize )
    if not ape.filepath_root( p, ape.FILEPATH_NATIVE ) then
      return ape.filepath_merge( ccwd, p, ape.FILEPATH_NATIVE )
    else
      local mycwd = cwd
      if normalize then
        mycwd, p = cwd_up, p:upper()
      end
      if mycwd and #mycwd <= #p and p:sub( 1, #mycwd ) == mycwd then
        local rel = p:sub( #mycwd+1 )
        while rel:sub( 1, 1 ) == "/" do
          rel = rel:sub( 2 )
        end
        while rel:sub( 1, #dirsep ) == dirsep do
          rel = rel:sub( 1+#dirsep )
        end
        return ape.filepath_merge( ".", rel, ape.FILEPATH_NATIVE )
      end
    end
  end
end
_M.get_path = get_path


local function get_pathat( pd, pid, atfd, name )
  if atfd == "AT_FDCWD" then
    return get_path( pd[ pid ].cwd, name )
  else
    local d = pd[ pid ][ atfd ] or pd[ pid ].cwd
    if d then
      return get_path( d, name )
    end
  end
end
_M.get_pathat = get_pathat


local function checked( func, td, pd, pid, ... )
  if pd[ pid ] then
    return func( td, pd, pid, ... )
  else
    local delayed = pd.delayed
    if type( delayed ) ~= "table" then
      delayed = {}
      pd.delayed = delayed
    end
    local pt = delayed[ pid ]
    if type( pt ) ~= "table" then
      pt = { { f = func, n = select( '#', ... ), ... } }
      delayed[ pid ] = pt
    else
      pt[ #pt+1 ] = { f = func, n = select( '#', ... ), ... }
    end
  end
end

local function replay( td, pd, pid )
  local delayed = pd.delayed
  if type( delayed ) == "table" then
    local pt = delayed[ pid ]
    if pt and pd[ pid ] then
      for i = 1, #pt do
        local v = pt[ i ]
        v.f( td, pd, pid, unpack( v, 1, v.n ) )
      end
      delayed[ pid ] = nil
    end
  end
end


local function open( td, pd, pid, name, dir, fd )
  local path = get_path( pd[ pid ].cwd, name )
  if path then
    if dir == "out" then
      td.output[ path ] = true
    elseif not td.output[ path ] then
      td.input[ path ] = true
    end
  end
  pd[ pid ][ fd ] = path or name
end

function _M.open( td, pd, pid, name, dir, fd )
  return checked( open, td, pd, pid, name, dir, fd )
end


local function openat( td, pd, pid, atfd, name, dir, fd )
  local path = get_pathat( pd, pid, atfd, name )
  if path then
    if dir == "out" then
      td.output[ path ] = true
    elseif not td.output[ path ] then
      td.input[ path ] = true
    end
  end
  pd[ pid ][ fd ] = path or name
end

function _M.openat( td, pd, pid, atfd, name, dir, fd )
  return checked( openat, td, pd, pid, atfd, name, dir, fd )
end


local function creat( td, pd, pid, name, fd )
  local path = get_path( pd[ pid ].cwd, name )
  if path then
    td.output[ path ] = true
  end
  pd[ pid ][ fd ] = path or name
end

function _M.creat( td, pd, pid, name, fd )
  return checked( creat, td, pd, pid, name, fd )
end


local function exec( td, pd, pid, prog )
  if prog ~= ape.basename( prog ) then
    local progpath = get_path( pd[ pid ].cwd, prog )
    if progpath and not td.output[ progpath ] then
      td.input[ progpath ] = true
    end
  end
end

function _M.exec( td, pd, pid, prog )
  return checked( exec, td, pd, pid, prog )
end


local function chdir( td, pd, pid, name )
  local path = get_path( pd[ pid ].cwd, name )
  pd[ pid ].cwd = path or name
end

function _M.chdir( td, pd, pid, name )
  return checked( chdir, td, pd, pid, name )
end


local function fchdir( td, pd, pid, fd )
  local name = pd[ pid ][ fd ]
  if name then
    return _M.chdir( td, pd, pid, name )
  end
end

function _M.fchdir( td, pd, pid, fd )
  return checked( fchdir, td, pd, pid, fd )
end


local function mkdir( td, pd, pid, name )
  local path = get_path( pd[ pid ].cwd, name )
  if path then
    td.output[ path ] = true
  end
end

function _M.mkdir( td, pd, pid, name )
  return checked( mkdir, td, pd, pid, name )
end


local function mkdirat( td, pd, pid, atfd, name )
  local path = get_pathat( pd, pid, atfd, name )
  if path then
    td.output[ path ] = true
  end
end

function _M.mkdirat( td, pd, pid, atfd, name )
  return checked( mkdirat, td, pd, pid, atfd, name )
end


local function fork( td, pd, ppid, cpid )
  local ppdata, cpdata = pd[ ppid ], {}
  for k,v in pairs( ppdata ) do
    cpdata[ k ] = v
  end
  pd[ cpid ] = cpdata
  replay( td, pd, cpid )
end

function _M.fork( td, pd, ppid, cpid )
  return checked( fork, td, pd, ppid, cpid )
end


local function rename( td, pd, pid, from, to )
  local fpath = get_path( pd[ pid ].cwd, from )
  local tpath = get_path( pd[ pid ].cwd, to )
  if fpath and not td.output[ fpath ] then
    td.input[ fpath ] = true
  end
  if tpath then
    td.output[ fpath ] = nil
    td.output[ tpath ] = true
  end
end

function _M.rename( td, pd, pid, from, to )
  return checked( rename, td, pd, pid, from, to )
end


local function renameat( td, pd, pid, atfd, from, atfd2, to )
  local fpath = get_pathat( pd, pid, atfd, from )
  local tpath = get_pathat( pd, pid, atfd2, to )
  if fpath and not td.output[ fpath ] then
    td.input[ fpath ] = true
  end
  if tpath then
    if fpath then
      td.output[ fpath ] = nil
    end
    td.output[ tpath ] = true
  end
end

function _M.renameat( td, pd, pid, atfd, from, atfd2, to )
  return checked( renameat, td, pd, pid, atfd, from, atfd2, to )
end


function _M.finish( deps, td )
  -- avoid rehashing of input files
  for k in pairs( td.input ) do
    if deps.input[ k ] then
      td.input[ k ] = deps.input[ k ]
    end
  end
  -- replace old deps with new ones
  deps.input = td.input
  deps.output = td.output
end

-- return module table
return _M

