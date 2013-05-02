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

local ape = require( "ape" ) -- (subset of) apache portable runtime
-- the module table
local _M = {}


-- convenience function for specifying multiple files as a single
-- string
function _M.qw( s )
  assert( type( s ) == "string" )
  local t = {}
  for x in s:gmatch( "%S+" ) do
    t[ #t+1 ] = x
  end
  return t
end


function _M.gsub( s, p, r )
  local t = type( s )
  if t == "table" then
    local o = {}
    for i = 1, #s do
      o[ i ] = _M.gsub( s[ i ], p, r )
    end
    return o
  elseif t == "string" then
    return (s:gsub( p, r ))
  else
    return s
  end
end


function _M.assert( check, reason )
  if not check then
    error( { type = "assert", reason = reason } )
  end
end


local function have_exec( lvl, ... )
  for i = 1, select( '#', ... ) do
    local f = select( i, ... )
    local ret, msg = ape.find_exec( f )
    if ret then
      return ret
    elseif msg then
      error( "find_exec'" .. tostring( f ) .."' = " .. msg, lvl )
    end
  end
end

function _M.have_exec( ... )
  return have_exec( 3, ... )
end


-- check that at least one of the given alternatives is executable
function _M.assert_exec( ... )
  local f = have_exec( 3, ... )
  if not f then
    error( { type = "exec", ... } )
  end
  return _M.run( f )  -- run is added by the main program
end


local function have_file( lvl, ... )
  for i = 1, select( '#', ... ) do
    local ret, msg = ape.find_file( (select( i, ... )) )
    if ret then
      return ret
    elseif msg then
      error( "find_file( ... ) = " .. msg, lvl )
    end
  end
end

function _M.have_file( ... )
  return have_file( 3, ... )
end


function _M.assert_file( ... )
  local f = have_file( 3, ... )
  if not f then
    error( { type = "file", ... } )
  end
  return f
end


function _M.glob( ... )
  local t, msg = {}
  for i = 1, select( '#', ... ) do
    local s = select( i, ... )
    t, msg = ape.path_glob( t, s )
    if not t then
      error( "glob'" .. tostring( s ) .. "' = " .. msg, 2 )
    end
  end
  return t
end


local function handle_return( ok, ... )
  if not ok then
    error( (...), 0 )
  end
  return ...
end

function _M.dofile( f, ... )
  local f, msg = loadfile( f )
  if not f then
    error( msg, 2 )
  end
  setfenv( f, {} )
  return handle_return( pcall( f, ... ) )
end


function _M.opt_dofile( f, ... )
  local res, msg = ape.find_file( f )
  if res then
    return _M.dofile( f, ... )
  elseif msg then
    error( "find_file'" .. tostring( f ) .. "' = " .. msg, 2 )
  end
end


function _M.basename( s, ... )
  local ret, msg = ape.basename( s, ... )
  if not ret then
    error( "basename'" .. tostring( s ) .. "' = " .. msg, 2 )
  end
  return ret
end
string.basename = _M.basename


function _M.dirname( s )
  local ret, msg = ape.dirname( s )
  if not ret then
    error( "dirname'" .. tostring( s ) .. "' = " .. msg, 2 )
  end
  return ret
end
string.dirname = _M.dirname


function _M.argv( s )
  s = s:gsub( "^%s*([^\n]*).*$", "%1" ) -- take first line only
  s = s:gsub( "%s*$", "" ) -- remove trailing whitespace
  local t, msg = ape.tokenize_to_argv( s )
  if not t then
    error( "tokenize_to_argv( ... ) = " .. msg, 2 )
  end
  return t
end


return _M

