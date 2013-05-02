local ape = require( "ape" ) -- (subset of) apache portable runtime
local bci = require( "bci" ) -- bytecode inspector library
local make = require( "make" ) -- useful functions for buildsh scripts
local dirsep = package.config:sub( 1, 1 )
local _G = _G
_G.make = make

local exec_handler, dependencies, depproxy, dont_save_deps


local function save_deps( deps )
  local f = io.open( ".deps.lua", "w" )
  if f then
    f:write( "return {\n" )
    for k,v in pairs( deps ) do
      if type( k ) == "string" and type( v ) == "table" then
        f:write( ("  [ %q ] = {\n"):format( k ) )
        if type( v.input ) == "table" then
          f:write( "    input = {\n" )
          for fn,h in pairs( v.input ) do
            if type( fn ) == "string" and type( h ) == "string" then
              f:write( ("      [ %q ] = %q,\n"):format( fn, h ) )
            end
          end
          f:write( "    },\n" )
        end
        if type( v.output ) == "table" then
          f:write( "    output = {\n" )
          for fn,h in pairs( v.output ) do
            if type( fn ) == "string" and type( h ) == "string" then
              f:write( ("      [ %q ] = %q,\n"):format( fn, h ) )
            end
          end
          f:write( "    },\n" )
        end
        f:write( "  },\n" )
      end
    end
    f:write( "}\n" )
    f:close()
  end
end


local function load_deps()
  local f, ok, t = loadfile( ".deps.lua" ), nil, nil
  if f then
    setfenv( f, {} )
    ok, t = pcall( f )
    if not ok or type( t ) ~= "table" then
      t = {}
    end
  else
    t = {}
  end
  local ud = newproxy( true )
  local m = getmetatable( ud )
  m.__gc = function()
    if not dont_save_deps then
      save_deps( t )
    end
  end
  return t, ud
end


-- handle commandline arguments
local function handle_args( args )
  local targets, files = {}, nil
  local n = 1
  if type( args[ 1 ] ) == "string" and
     args[ 1 ]:match( "make%.[^/\\]-%.lua$" ) then
    files = { args[ 1 ] }
    n = 2
  else
    files = assert( ape.match_glob( "make.*.lua" ) )
  end
  while type( args[ n ] ) == "string" do
    targets[ #targets+1 ] = args[ n ]
    n = n + 1
  end
  if #targets == 0 then
    targets[ 1 ] = "build"
  end
  return files, targets
end


-- check the bytecode of a loaded function to find getglobal and
-- setglobal opcodes
local function check_bytecode( f, gt, st, special )
  local h = bci.getheader( f )
  local last_dollar, last_loadk, loadk_val
  for i = 1, h.instructions do
    local line, code, _, const = bci.getinstruction( f, i )
    if code == "CALL" and last_loadk == i-1 and last_dollar == i-2 then
      if not gt[ loadk_val ] then
        gt[ loadk_val ] = line
        special[ loadk_val ] = true
      end
    else
      local name
      if const ~= nil and const < 0 then
        name = bci.getconstant( f, -const )
      end
      if name then
        if code == "GETGLOBAL" then
          if name == "$" then
            last_dollar = i
          elseif name:sub( 1, 1 ) == "$" then
            local n = name:sub( 2 )
            gt[ n ] = line
            special[ n ] = nil
          end
        elseif code == "SETGLOBAL" then
          st[ name ] = line
        elseif code == "LOADK" then
          last_loadk = i
          loadk_val = name
        end
      end
    end
  end
  for i = 1, h.functions do
    check_bytecode( bci.getfunction( f, i ), gt, st, special )
  end
  return gt, st, special
end


local err = io.stderr

local function write_err( fname, line, ... )
  err:write( "-- " )
  if fname then
    err:write( fname )
    if line then
      err:write( ":", tostring( line ) )
    end
    err:write( ": " )
  end
  err:write( ... )
  err:write( "\n" )
end


local function write_list( list, del )
  if #list > 0 then
    local prefix = "--    "
    del = del or ","
    local i, s = 1, "`" .. list[ 1 ] .. "'"
    while i < #list do
      local s2 = "`" .. list[ i+1 ] .. "'"
      if #s + #s2 + #prefix + #del + 1 <= 70 then
        s = s .. del .. " " .. s2
      else
        err:write( prefix, s, del, "\n" )
        s = s2
      end
      i = i + 1
    end
    err:write( prefix, s, "\n" )
  end
end


local function list_targets( res )
  local tgts = {}
  for k in pairs( res ) do
    tgts[ #tgts+1 ] = k
  end
  table.sort( tgts )
  err:write( "== listing available targets:\n" )
  write_list( tgts, "," )
end


local function make_env( progs, special_gg )
  local special_env = {}
  local function dollar( p )
    local f = special_env[ p ]
    if not f then
      f = make.run( p )
      special_env[ p ] = f
    end
    return f
  end
  local env = { [ "$" ] = dollar }
  for name in pairs( progs ) do
    local rp = make.run( name )
    special_env[ name ] = rp
    if not special_gg[ name ] then
      env[ "$"..name ] = rp
    end
  end
  setmetatable( env, { __index = _G } )
  return env
end


local function make_env_with_executables( fname, gg, special_gg )
  local missing = {}
  for gg_name, gg_line in pairs( gg ) do
    local p, msg = ape.find_exec( gg_name )
    if p == nil and msg then
      write_err( fname, nil, "find_exec'", ggname, "' = ", msg )
      return false, false
    elseif p == nil then
      missing[ #missing+1 ] = gg_name
    end
  end
  if #missing > 0 then
    write_err( fname, nil, "executable(s) not found in PATH:" )
    write_list( missing, "," )
    return false, true
  else
    return true, true, make_env( gg, special_gg )
  end
end


local function call_buildsh_function( fname, f, is_target )
  local cont = true
  local ok, res_or_msg = pcall( f )
  if not ok then
    if type( res_or_msg ) == "table" then
      if res_or_msg.type == "file" then
        if is_target then
          write_err( fname, nil,
                     "assert_file called in target function. missing:" )
          cont = false
        else
          if #res_or_msg > 1 then
            write_err( fname, nil, "required file missing (one of):" )
          else
            write_err( fname, nil, "required file missing:" )
          end
        end
        write_list( res_or_msg, ", or" )
      elseif res_or_msg.type == "exec" then
        if is_target then
          write_err( fname, nil,
                     "assert_exec called in target function. missing:" )
          cont = false
        else
          if #res_or_msg > 1 then
            write_err( fname, nil, "required program missing (one of):" )
          else
            write_err( fname, nil, "required program missing:" )
          end
        end
        write_list( res_or_msg, ", or" )
      elseif res_or_msg.type == "assert" then
        if is_target then
          write_err( fname, nil,
                     "assert called in target function. reason:" )
        else
          write_err( fname, nil, "build assertion failed:" )
        end
        err:write( "--    ", res_or_msg.reason or "unknown reason", "\n" )
      else
        write_err( fname, nil, "unknown/unexpected error value" )
        cont = false
      end
    else
      write_err( nil, nil, tostring( res_or_msg ) )
      cont = false
    end
    return false, cont
  end
  return true, true, res_or_msg
end


-- function for creating a suitable argument list from nested arrays
-- of values
local flatten
do
  local function my_flatten( t, n, v )
    if v ~= nil then
      if type( v ) == "table" then
        for i = 1, #v do
          t, n = my_flatten( t, n, v[ i ] )
        end
      else
        n = n + 1
        t[ n ] = v
      end
    end
    return t, n
  end

  function flatten( ... )
    local t, n = {}, 0
    for i = 1, select( '#', ... ) do
      t, n = my_flatten( t, n, (select( i, ... )) )
    end
    return t, n
  end
end


local argv2cmd
do
  local a2c_pattern = dirsep == "\\" and "[%s\"]" or "[%s\\\"]"
  local a2c_repl = {
    [ "\\" ] = "\\\\",
    [ '"' ] = '\\"',
    [ "\t" ] = " ",
    [ "\n" ] = " ",
    [ "\r" ] = "",
  }

  function argv2cmd( argv )
    local s = ""
    for i = 1, #argv do
      local a = argv[ i ]
      if a:match( a2c_pattern ) then
        a = '"' .. a:gsub( a2c_pattern, a2c_repl ) .. '"'
      end
      s = s .. a
      if i < #argv then
        s = s .. " "
      end
    end
    return s
  end
end


local hash_file
do
  local hasher = ape.sha256_new( ape.pool_create() )

  function hash_file( filename )
    hasher:reset()
    local ok, msg = ape.hash_file( hasher, filename )
    if ok then
      return hasher:digest()
    else
      return msg
    end
  end
end


local function deep_copy( v )
  if type( v ) == "table" then
    local t = {}
    for k,v in pairs( v ) do
      t[ k ] = deep_copy( v )
    end
    return t
  end
  return v
end


local function update_deps_io( deps_io, onlynew, stop )
  local differ = false
  for fn,ohash in pairs( deps_io ) do
    if not onlynew or type( ohash ) ~= "string" then
      local nhash = hash_file( fn )
      deps_io[ fn ] = nhash
      if ohash ~= nhash then
        differ = true
        if stop then return true end
      end
    end
  end
  return differ
end


local function check_deps( depstable, cmd )
  local run_it = false
  local deps = depstable[ cmd ]
  if type( deps ) ~= "table" then
    deps = { input = {}, output = {} }
    run_it = true
  else
    deps = deep_copy( deps )
  end
  if type( deps.input ) ~= "table" then
    deps.input = {}
    run_it = true
  end
  if type( deps.output ) ~= "table" then
    deps.output = {}
    run_it = true
  end
  if next( deps.input ) == nil and next( deps.output ) == nil then
    run_it = true
  end
  if not run_it then
    run_it = update_deps_io( deps.input )
    if not run_it then
      run_it = update_deps_io( deps.output, false, true )
    end
  end
  return deps, run_it
end


function make.run( p )
  return function( a, ... )
    local p = p
    local argv = flatten( p, a, ... )
    local sargv = argv2cmd( argv )
    local deps, run_it = check_deps( dependencies, sargv )
    if run_it then
      dont_save_deps = nil
      if type( a ) == "table" and type( a.echo ) == "string" then
        local bn = ape.basename( p, ".exe", ".cmd", ".bat" )
        if bn then
          err:write( "[", bn, "] ", a.echo, "\n" )
        else
          err:write( sargv, "\n" )
        end
      else
        err:write( sargv, "\n" )
      end
      local data
      if type( exec_handler ) == "table" and
         type( exec_handler.pre_process ) == "function" then
        argv, data = exec_handler.pre_process( argv )
        p = argv[ 1 ]
      end
      local pool = ape.pool_create()
      local pattr, msg = ape.procattr_create( pool )
      if not pattr then error( "exec'" .. p .. "' = " .. msg, 2 ) end
      local ok, msg = pattr:cmdtype_set( "program/path" )
      if not ok then error( "exec'" .. p .. "' = " .. msg, 2 ) end
      ok, msg = pattr:io_set( "file", "file", "file" )
      if not ok then error( "exec'" .. p .. "' = " .. msg, 2 ) end
      if type( a ) == "table" and type( a.dir ) == "string" then
        ok, msg = pattr:dir_set( a.dir )
        if not ok then error( "exec'" .. p .. "' = " .. msg, 2 ) end
      end
      local proc, msg = ape.proc_create( p, argv, nil, pattr )
      if not proc then error( "exec'" .. p .. "' = " .. msg, 2 ) end
      local ok, etype, code  = proc:wait( "wait" )
      if not ok then
        if etype == "exit" then
          error( "program `" .. p .. "' exited with status code " .. tostring( code ), 2 )
        elseif etype == "signal" then
          error( "program `" .. p .. "' died from signal " .. tostring( code ), 2 )
        else
          error( "exec'" .. p .. "' = " .. etype, 2 )
        end
      else
        if type( exec_handler ) == "table" and
           type( exec_handler.post_process ) == "function" then
          local dir = "."
          if type( a ) == "table" and type( a.dir ) == "string" then
            dir = a.dir
          end
          exec_handler.post_process( deps, data, dir )
          update_deps_io( deps.input, true, false )
          update_deps_io( deps.output )
          dependencies[ sargv ] = deps
        end
      end
    end
  end
end


function make.pipe( p )
  local f = make.have_exec( p )
  if not f then
    error( { type = "exec", p } )
  end
  return function( ... )
    local argv = flatten( p, ... )
    err:write( argv2cmd( argv ), "\n" )
    local ok, etype, code, output = ape.run_collect( "program/path", p, argv )
    if ok then
      return output
    else
      if etype == "exit" then
        error( "program `" .. p .. "' exited with status code " .. tostring( code ), 2 )
      elseif etype == "signal" then
        error( "program `" .. p .. "' died from signal " .. tostring( code ), 2 )
      else
        error( "exec'" .. p .. "' = " .. etype, 2 )
      end
    end
  end
end


local collect_outputs
do
  --[[
  local function compare( a, b )
    local a_len, b_len = #a, #b
    if a_len > b_len then
      return true
    elseif a_len == b_len then
      return a > b
    else
      return false
    end
  end
  --]]
  local function compare( a, b )
    return a > b
  end

  function collect_outputs( deps )
    local t = {}
    if type( deps ) == "table" then
      for _,v in pairs( deps ) do
        if type( v ) == "table" and
           type( v.output ) == "table" then
          for o in pairs( v.output ) do
            if type( o ) == "string" then
              t[ #t+1 ] = o
            end
          end
        end
      end
    end
    table.sort( t, compare )
    return t
  end
end


function make.autoclean()
  err:write( "== cleaning up ...\n" )
  dont_save_deps = true
  local outputs = collect_outputs( dependencies )
  outputs[ #outputs+1 ] = ".deps.lua"
  for _,f in ipairs( outputs ) do
    write_err( nil, nil, "deleting `" .. f .. "' ..." )
    if not os.remove( f ) then
      ape.dir_remove( f )
    end
  end
  for k in pairs( dependencies ) do
    dependencies[ k ] = nil
  end
end



-- load/initialize dependencies table
dependencies, depproxy = load_deps()
-- figure out which syscall tracing method to use
do
  local platform = ape.platform()
  local preload_env = os.getenv( "BUILDSH_PRELOAD" )

  if (platform == "UNIX" or platform == "MACOSX") and preload_env and
     make.have_file( preload_env ) then
    exec_handler = require( "preload" )
  elseif platform == "UNIX" and make.have_exec( "strace" ) then
    exec_handler = require( "strace" )
  elseif (platform == "UNIX" or platform == "MACOSX") and
         make.have_exec( "ktrace" ) and make.have_exec( "kdump" ) then
    exec_handler = require( "ktrace" )
  elseif platform == "WINDOWS" and make.have_exec( "tracker.exe" ) and
         ape.hacks and ape.hacks.ucs2_to_utf8 then
    exec_handler = require( "tracker" )
  end
--[[
  -- TODO add more syscall tracing tools here!
  -- TODO dll injection on windows?
  -- XXX truss misses *lots* of syscalls on pc-bsd
  -- XXX dtruss/dtrace requires root privileges
--]]
end

-- start main program
local make_files, make_targets = handle_args( arg )
local retval = true
for _,fname in ipairs( make_files ) do
  err:write( "== checking `", fname, "' ...\n" )
  local f, msg = loadfile( fname )
  if not f then
    retval = false
    write_err( nil, nil, msg )
  else
    local gg, sg, special_gg = check_bytecode( f, {}, {}, {} )
    local sg_name, sg_line = next( sg )
    if sg_name then
      retval = false
      write_err( fname, sg_line, "no globals assignment allowed (`",
                 sg_name, "')" )
    else
      local ok, cont, env = make_env_with_executables( fname, gg, special_gg )
      if not ok then
        if not cont then
          return false
        end
        retval = false
      else
        setfenv( f, env )
        local ok, cont, res = call_buildsh_function( fname, f )
        if not ok then
          if not cont then
            return false
          end
          retval = false
        elseif type( res ) ~= "table" or
             next( res ) == nil then
          write_err( fname, nil, "no targets defined" )
          return false
        else -- got target definition table
          retval = true
          err:write( "== `", fname, "' it is ...\n" )
          if exec_handler then
            err:write( "== using ", exec_handler.name,
                       " to detect dependencies.\n" )
          end
          for i = 1, #make_targets do
            local target = make_targets[ i ]
            local tfunc = res[ target ]
            if type( tfunc ) ~= "function" and
               (target == "list" or target == "clean") then
              if target == "list" then
                list_targets( res )
              else
                make.autoclean()
              end
            else
              err:write( "== executing target `", target, "' ...\n" )
              if type( tfunc ) ~= "function" then
                write_err( fname, nil, "no target `", target, "' defined" )
                return false
              elseif not call_buildsh_function( fname, tfunc, true ) then
                return false
              end
            end
          end
          if retval then
            err:write( "== done.\n" )
          end
          return retval
        end
      end
    end
  end
end

return retval

