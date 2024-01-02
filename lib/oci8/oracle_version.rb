# oracle_version.rb implements OCI8::OracleVersion.
#
# Copyright (C) 2009-2013 Kubo Takehiro <kubo@jiubao.org>

#
class OCI8

  # The data class, representing Oracle version.
  #
  # Oracle version is represented by five numbers:
  # *major*, *minor*, *update*, *patch* and *port_update*.
  #
  # @see OCI8.oracle_client_version
  # @see OCI8#oracle_server_version
  class OracleVersion
    include Comparable

    # The first part of the Oracle version.
    attr_reader :major
    # The second part of the Oracle version.
    attr_reader :minor
    # The third part of the Oracle version.
    attr_reader :update
    # The fourth part of the Oracle version.
    attr_reader :patch
    # The fifth part of the Oracle version.
    attr_reader :port_update

    # Creates an OCI8::OracleVersion object.
    #
    # If the first argument _arg_ is a String, it is parsed as dotted
    # version string. If it is bigger than 0x08000000, it is parsed as
    # a number contains 5-digit Oracle version. Otherwise, it is used
    # as a major version and the rest arguments are minor, update,
    # patch and port_update. Unspecified version numbers are zeros by
    # default.
    #
    # @example
    #   # When the first argument is a String,
    #   oraver = OCI8::OracleVersion.new('11.2.0.3')
    #   oraver.major       # => 11
    #   oraver.minor       # =>  2
    #   oraver.update      # =>  0
    #   oraver.patch       # =>  3
    #   oraver.port_update # =>  0
    #
    #   # When the first argument is bigger than 0x08000000,
    #   oraver = OCI8::OracleVersion.new(0x0b200300)
    #   oraver.major       # => 11
    #   oraver.minor       # =>  2
    #   oraver.update      # =>  0
    #   oraver.patch       # =>  3
    #   oraver.port_update # =>  0
    #
    #   # Otherwise,
    #   oraver = OCI8::OracleVersion.new(11, 2, 0, 3)
    #   oraver.major       # => 11
    #   oraver.minor       # =>  2
    #   oraver.update      # =>  0
    #   oraver.patch       # =>  3
    #   oraver.port_update # =>  0
    #
    # @return [OCI8::OracleVersion]
    def initialize(arg, minor = nil, update = nil, patch = nil, port_update = nil, is_server_version: false)
      if arg.is_a? String
        major, minor, update, patch, port_update = arg.split('.').collect do |v|
          v.to_i
        end
      elsif arg >= 0x12000000
        major  = (arg & 0xFF000000) >> 24
        minor  = (arg & 0x00FF0000) >> 16
        update = (arg & 0x0000F000) >> 12
        patch  = (arg & 0x00000FF0) >>  4
        port_update = (arg & 0x0000000F)
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
      @vernum = if @major >= 18
                  (@major << 24) | (@minor << 16) | (@update << 12) | (@patch << 4) | @port_update
                else
                  (@major << 24) | (@minor << 20) | (@update << 12) | (@patch << 8) | @port_update
                end
      @is_server_version = is_server_version
    end

    # Compares +self+ and +other+.
    #
    # <=> is the basis for the methods <, <=, ==, >, >=, and between?,
    # included from the Comparable module.
    #
    # @return [-1, 0, +1]
    def <=>(other)
      @vernum <=> other.to_i
    end

    # Returns an integer number contains 5-digit Oracle version.
    #
    # If the hexadecimal notation is 0xAABCCDEE, *major*, *minor*,
    # *update*, *patch* and *port_update* are 0xAA, 0xB, 0xCC, 0xD and
    # 0xEE respectively.
    #
    # @example
    #   oraver = OCI8::OracleVersion.new('11.2.0.3')
    #   oraver.to_i          # => 186647296
    #   '%08x' % oraver.to_i # => "0b200300"
    #
    # @return [Integer]
    def to_i
      @vernum
    end

    # Returns a dotted version string of the Oracle version.
    #
    # @example
    #   oraver = OCI8::OracleVersion.new('11.2.0.3')
    #   oraver.to_s          # => '11.2.0.3.0'
    #
    # @return [String]
    def to_s
      if @is_server_version && @major >= 23
        format('%d.%d.%d.%d.%02d', @major, @minor, @update, @patch, @port_update)
      else
        format('%d.%d.%d.%d.%d', @major, @minor, @update, @patch, @port_update)
      end
    end

    # Returns true if +self+ and +other+ are the same type and have
    # equal values.
    #
    # @return [true or false]
    def eql?(other)
      other.is_a? OCI8::OracleVersion and (self <=> other) == 0
    end

    # Returns a hash based on the value of +self+.
    #
    # @return [Integer]
    def hash
      @vernum
    end

    # @private
    def inspect
      "#<#{self.class.to_s}: #{self.to_s}>"
    end
  end
end
