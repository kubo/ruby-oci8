# properties.rb -- implements OCI8.properties
#
# Copyright (C) 2010-2011 KUBO Takehiro <kubo@jiubao.org>

class OCI8

  @@properties = {
    :length_semantics => :byte,
    :bind_string_as_nchar => false,
    :float_conversion_type => :ruby,
  }

  def @@properties.[](name)
    raise IndexError, "No such property name: #{name}" unless @@properties.has_key?(name)
    super(name)
  end

  def @@properties.[]=(name, val)
    raise IndexError, "No such property name: #{name}" unless @@properties.has_key?(name)
    case name
    when :length_semantic
      if val != :byte and val != char
        raise ArgumentError, "Invalid property value #{val} for :length_semantics."
      end
    when :bind_string_as_nchar
      val = val ? true : false
    when :float_conversion_type
      # handled by native code in oci8lib_xx.so.
      OCI8.__set_property(name, val)
    end
    super(name, val)
  end

  # call-seq:
  #   OCI8.properties -> a customized Hash
  #
  # (new in 2.0.5)
  #
  # Returns a Hash which ruby-oci8 global settings.
  # The hash's setter and getter methods are customized to check
  # property names and values.
  #
  #   # get properties
  #   OCI8.properties[:bind_string_as_nchar]  # => false
  #   OCI8.properties[:invalid_property_name] # raises an IndexError
  #
  #   # set properties
  #   OCI8.properties[:bind_string_as_nchar] = true
  #   OCI8.properties[:invalid_property_name] = true # raises an IndexError
  #
  # Supported properties are listed below:
  #
  # [:length_semantics]
  #     +:char+ when Oracle character length is counted by the number of characters.
  #     +:byte+ when it is counted by the number of bytes.
  #     The default setting is +:byte+ because +:char+ causes unexpected behaviour on
  #     Oracle 9i.
  #
  # [:bind_string_as_nchar]
  #     +true+ when string bind variables are bound as NCHAR,
  #     otherwise +false+. The default value is +false+.
  #
  # [:float_conversion_type]
  #     (new in 2.1.0)
  #     Specifies who converts decimal to float and vice versa.
  #     It should be either +:ruby+ or +:oracle+. The default value
  #     is +:ruby+.
  #     See: http://rubyforge.org/forum/forum.php?thread_id=50030&forum_id=1078
  #
  def self.properties
    @@properties
  end
end
