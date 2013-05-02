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
  local odir = os.tmpname()
  local newargv = { "tracker.exe", "/e", "/if", odir, "/c" }
  local n = #newargv
  for i = 1,#argv do
    newargv[ n+i ] = argv[ i ]
  end
  return newargv, odir
end


local be_bom, le_bom = string.char( 254, 255 ), string.char( 255, 254 )
local be_lf, le_lf = string.char( 0, 10 ), string.char( 10, 0 )
local cr = ("\r"):byte()

local function read_utf16_line( f, is_first_line, is_be )
  local t, lf = {}, le_lf
  local s = f:read( 2 )
  if is_first_line then
    if s == be_bom then
      is_be = true
      s = f:read( 2 )
    elseif s == le_bom then
      is_be = false
      s = f:read( 2 )
    end
  end
  if is_be then lf = be_lf end
  while s and #s == 2 and s ~= lf do
    local a, b = s:byte( 1, 2 )
    if is_be then
      t[ #t+1 ] = a*256 + b
    else
      t[ #t+1 ] = b*256 + a
    end
    s = f:read( 2 )
  end
  if t[ #t ] == cr then
    t[ #t ] = nil
  end
  if s == lf or #t > 0 then
    return t, is_be
  else
    return nil, is_be
  end
end


local ucs2_to_utf8 = ape.hacks.ucs2_to_utf8

local function each_line( file, func, ... )
  local h = io.open( file, "rb" )
  if h then
    local line, is_be = read_utf16_line( h, true )
    -- skip first line
    if line then line = read_utf16_line( h, false, is_be ) end
    while line do
      line = ucs2_to_utf8( line )
      if line then
        func( line, ... )
      end
      line = read_utf16_line( h, false, is_be )
    end
    h:close()
  end
end


local function handle_write( line, td )
  local p = base.get_path( ".", line, true )
  if p then
    td.output[ p ] = true
  end
end

local function handle_delete( line, td )
  local p = base.get_path( ".", line, true )
  if p then
    td.output[ p ] = nil
  end
end

local function handle_read( line, td )
  local p = base.get_path( ".", line, true )
  if p and not td.output[ p ] then
    td.input[ p ] = true
  end
end


local function post_process( deps, data )
  local tempdeps = { input = {}, output = {} }
  -- scan files in the temp dir
  local t = ape.path_glob( data .. "/*.write.*.tlog" )
  if t then
    for i = 1, #t do
      each_line( t[ i ], handle_write, tempdeps )
    end
  end
  t = ape.path_glob( data .. "/*.delete.*.tlog" )
  if t then
    for i = 1, #t do
      each_line( t[ i ], handle_delete, tempdeps )
    end
  end
  t = ape.path_glob( data .. "/*.read.*.tlog" )
  if t then
    for i = 1, #t do
      each_line( t[ i ], handle_read, tempdeps )
    end
  end
  -- remove temp dir
  t = ape.path_glob( data .. "/*.tlog" )
  if t then
    for i = 1, #t do
      os.remove( t[ i ] )
    end
  end
  ape.dir_remove( data )
  -- avoid rehashing of input files
  base.finish( deps, tempdeps )
end

return {
  name = "tracker.exe",
  pre_process = pre_process,
  post_process = post_process,
}

