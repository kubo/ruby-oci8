module RUNIT
  module RObserver
    ADD_ERROR = :add_error
    ADD_FAILURE = :add_failure
    START_TEST = :start_test
    END_TEST = :end_test

    def add_error(*arg)
    end

    def add_failure(*arg)
    end 

    def start_test(*arg)
    end

    def end_test(*arg)
    end

    def update(ev, *arg)
      send(ev, *arg)
    end

  end
end
