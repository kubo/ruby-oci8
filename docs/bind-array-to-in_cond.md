# @title Bind an Array to IN-condition

Bind an Array to IN-condition
=============================

Binding an arbitrary-length array to IN-condition is not simple.
You need to create an SQL statement containing a comma-separated
list whose length is same with the input data.

Example:

    ids = [ ... ] # an arbitrary-length array containing user IDs.
    
    place_holder_string = Array.new(ids.length) {|index| ":id_#{index}"}.join(', ')
    # place_holder_string is:
    #   ":id_0" if ids.length == 1
    #   ":id_0, :id_1" if ids.length == 2
    #     ...
    cursor = conn.parse("select * from users where id in (#{place_holder_string})")
    ids.each_with_index do |id, index|
      cursor.bind_param("id#{index}", id) # bind each element
    end
    cursor.exec()

However this is awkward. So {OCI8.in_cond} was added in ruby-oci8 2.2.2.
The above code is rewritten as follows:

    ids = [ ... ] # an arbitrary-length array containing user IDs.
    
    in_cond = OCI8::in_cond(:id, ids)]
    cursor = conn.exec("select * from users where id in (#{in_cond.names})", *in_cond.values)

or

    ids = [ ... ] # an arbitrary-length array containing user IDs.
    
    in_cond = OCI8::in_cond(:id, ids, Integer) # set the data type explicitly
    cursor = conn.exec("select * from users where id in (#{in_cond.names})", *in_cond.values)
