require 'yaml'
require 'erb'

class ArgDef
  attr_reader :dcl
  attr_reader :name

  def initialize(arg)
    /(\w+)\s*$/ =~ arg
    /\(\*(\w+)\)/ =~ arg if $1.nil?
    @dcl = arg
    @name = $1
  end
end

class FuncDef
  attr_reader :name
  attr_reader :version
  attr_reader :version_num
  attr_reader :version_str
  attr_reader :remote
  attr_reader :args
  attr_reader :ret
  attr_reader :code_if_not_found

  def initialize(key, val)
    if key[-3..-1] == '_nb'
      @name = key[0..-4]
      @remote = true
    else
      @name = key
      @remote = false
    end
    ver = val[:version]
    ver_major = (ver / 100)
    ver_minor = (ver / 10) % 10
    ver_update = ver % 10
    @version = ((ver_major << 24) | (ver_minor << 20) | (ver_update << 12))
    case @version
    when 0x08000000; @version_num = 'ORAVER_8_0'
    when 0x08100000; @version_num = 'ORAVER_8_1'
    when 0x09000000; @version_num = 'ORAVER_9_0'
    when 0x09200000; @version_num = 'ORAVER_9_2'
    when 0x0a100000; @version_num = 'ORAVER_10_1'
    when 0x0a200000; @version_num = 'ORAVER_10_2'
    when 0x0b100000; @version_num = 'ORAVER_11_1'
    end
    @version_str = "#{ver_major}.#{ver_minor}.#{ver_update}"
    @ret = val[:ret] || 'sword'
    @args = val[:args].collect do |arg|
      ArgDef.new(arg)
    end
    @code_if_not_found = val[:code_if_not_found]
  end
end

def create_apiwrap
  funcs = []
  YAML.load(open(File.dirname(__FILE__) + '/apiwrap.yml')).each do |key, val|
    funcs << FuncDef.new(key, val)
  end
  funcs.sort! do |a, b|
    if a.version == b.version
      a.name <=> b.name
    else
      a.version <=> b.version
    end
  end

  header_comment = <<EOS.chomp
/*
 * This file was created by apiwrap.rb.
 * Don't edit this file manually.
 */
EOS
#'

  erb = ERB.new(open(File.dirname(__FILE__) + '/apiwrap.h.tmpl').read)
  open('apiwrap.h', 'w') do |fd|
    fd.write(erb.result(binding))
  end

  erb = ERB.new(open(File.dirname(__FILE__) + '/apiwrap.c.tmpl').read)
  open('apiwrap.c', 'w') do |fd|
    fd.write(erb.result(binding))
  end
end

if $0 == __FILE__
  create_apiwrap
end
