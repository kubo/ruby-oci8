require 'runit/method_mappable'
module RUNIT
  module Setuppable
    include MethodMappable
    @@setups = {}
    def attach_setup(setup_method, *methods)
      attach_method(@@setups, setup_method, *methods)
    end
    private :attach_setup
    def invoke_setup(m)
      invoke_method(@@setups, m)
    end
    private :invoke_setup
  end
end
