module RUNIT
  module MethodMappable
    def attach_method(hash, method, *methods)
      methods.each do |m|
        hash[self.to_s + m.to_s] = method
      end
    end
    private :attach_method

    def invoke_method(hash, m)
      if hash[self.to_s + m.to_s]
        send hash[self.to_s + m.to_s]
      elsif hash[self.class.to_s + m.to_s]
        send hash[self.class.to_s + m.to_s]
      end
    end
    private :invoke_method
  end
end

