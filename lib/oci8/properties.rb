# properties.rb -- implements OCI8.properties
#
# Copyright (C) 2010 KUBO Takehiro <kubo@jiubao.org>

class OCI8

  @@properties = {
    :bind_string_as_nchar => false,
  }

  def @@properties.[](name)
    raise IndexError, "No such property name: #{name}" unless @@properties.has_key?(name)
    super(name)
  end

  def @@properties.[]=(name, val)
    raise IndexError, "No such property name: #{name}" unless @@properties.has_key?(name)
    case name
    when :bind_string_as_nchar
      val = val ? true : false
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
  # [:bind_string_as_nchar]
  #     +true+ when string bind variables are bound as NCHAR,
  #     otherwise +false+. The default value is +false+.
  def self.properties
    @@properties
  end
end
