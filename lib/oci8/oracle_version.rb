# oracle_version.rb implements OCI8::OracleVersion.
#
# Copyright (C) 2009 KUBO Takehiro <kubo@jiubao.org>

#--

class OCI8

  # A data class, representing Oracle version.
  #
  # Oracle version is represented by five numbers:
  # *major*, *minor*, *update*, *patch* and *port_update*.
  class OracleVersion
    include Comparable

    # The first part of the Oracle version.
    attr_reader :major
    # The second part of the Oracle version.
    attr_reader :minor
    # The third part of the Oracle version.
    attr_reader :update
    # The fifth part of the Oracle version.
    attr_reader :patch
    # The fourth part of the Oracle version.
    attr_reader :port_update

    # Creates a OCI8::OracleVersion object.
    #
    # If the first argument _arg_ is a String, it is parsed as dotted
    # version string. If it is bigger than 0x08000000, it is parsed as
    # a number contains 5-digit Oracle version. Otherwise, it is used
    # as a major version and the rest arguments are minor, update,
    # patch and port_update. Unspecified version numbers are zeros by
    # default.
    #
    # == Example
    #   oraver = OCI8::OracleVersion.new('10.2.0.4')
    #   oraver.major       # => 10
    #   oraver.minor       # =>  2
    #   oraver.update      # =>  0
    #   oraver.patch       # =>  4
    #   oraver.port_update # =>  0
    #
    #   oraver = OCI8::OracleVersion.new(0x0a200400)
    #   oraver.major       # => 10
    #   oraver.minor       # =>  2
    #   oraver.update      # =>  0
    #   oraver.patch       # =>  4
    #   oraver.port_update # =>  0
    def initialize(arg, minor = nil, update = nil, patch = nil, port_update = nil)
      if arg.is_a? String
        major, minor, update, patch, port_update = arg.split('.').collect do |v|
          v.to_i
        end
      elsif arg >= 0x08000000
        major  = (arg & 0xFF000000) >> 24
        minor  = (arg & 0x00F00000) >> 20
        update = (arg & 0x000FF000) >> 12
        patch  = (arg & 0x00000F00) >> 8
        port_update = (arg & 0x000000FF)
      else
        major = arg
      end
      @major = major
      @minor = minor || 0
      @update = update || 0
      @patch = patch || 0
      @port_update = port_update || 0
    end

    # :call-seq:
    #   oraver <=> other_oraver -> -1, 0, +1
    #
    # Compares +oraver+ and +other_oraver+.
    #
    # <=> is the basis for the methods <, <=, ==, >, >=, and between?,
    # included from module Comparable.
    def <=>(other)
      cmp = @major <=> other.major
      return cmp if cmp != 0
      cmp = @minor <=> other.minor
      return cmp if cmp != 0
      cmp = @update <=> other.update
      return cmp if cmp != 0
      cmp = @patch <=> other.patch
      return cmp if cmp != 0
      @port_update <=> other.port_update
    end

    # :call-seq:
    #   oraver.to_i -> integer
    #
    # Returns an integer number contains 5-digit Oracle version.
    #
    # If the hexadecimal notation is 0xAABCCDEE, *major*, *minor*,
    # *update*, *patch* and *port_update* are 0xAA, 0xB, 0xCC, 0xD and
    # 0xEE respectively.
    #
    # == Example
    #   oraver = OCI8::OracleVersion.new('10.2.0.4')
    #   oraver.to_i          # => 169870336
    #   '%08x' % oraver.to_i # => "0a200400"
    def to_i
      (@major << 24) | (@minor << 20) | (@update << 12) | (@patch << 8) | @port_update
    end

    # :call-seq:
    #   oraver.to_s -> string
    #
    # Returns a dotted version string of the Oracle version.
    #
    # == Example
    #   oraver = OCI8::OracleVersion.new('10.2.0.4')
    #   oraver.to_s          # => '10.2.0.4.0'
    def to_s
      format('%d.%d.%d.%d.%d', @major, @minor, @update, @patch, @port_update)
    end

    # :call-seq:
    #   oraver.eql? other -> true or false
    #
    # Returns true if +oraver+ and +other+ are the same type and have
    # equal values.
    #--
    # This is for class Hash to test members for equality.
    def eql?(other)
      other.is_a? OCI8::OracleVersion and (self <=> other) == 0
    end

    # :call-seq:
    #   oraver.hash -> integer
    #
    # Returns a hash based on the value of +oraver+.
    #--
    # This is for class Hash.
    def hash
      to_i
    end

    def inspect # :nodoc:
      "#<#{self.class.to_s}: #{self.to_s}>"
    end
  end
end
