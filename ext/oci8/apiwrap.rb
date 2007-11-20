require 'yaml'
require 'erb'

class ArgDef
  attr_reader :dcl
  attr_reader :name

  def initialize(arg)
    /(\w+)\s*$/ =~ arg
    @dcl = arg
    @name = $1
  end
end

class FuncDef
  attr_reader :name
  attr_reader :type
  attr_reader :args

  def initialize(f)
    @name = f[:name]
    @type = f[:type]
    @args = f[:args].collect do |arg|
      ArgDef.new(arg)
    end
  end
end

def create_apiwrap
  funcs = YAML.load(open(File.dirname(__FILE__) + '/apiwrap.yml'))

  funcs.collect! do |f|
    FuncDef.new(f)
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
