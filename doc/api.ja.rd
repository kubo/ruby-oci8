=begin
= Ruby/OCI8 上位API
[ ((<Home|URL:index.ja.html>)) ] [ ((<English|URL:api.en.html>)) | Japanese ]

Ruby/OCI8 は2階層の API に分かれます。ひとつは"下位API"で、もう一つは"
上位API"です。ここでは上位APIの使用法を解説します。本来は下位APIに所属
するクラスでも、上位API を使用する上で必要なクラスはここで解説します。

"上位API"は ruby で書かれたライブラリで、"下位API"の上に構築されていま
す。複雑な OCI の構造を隠蔽して、なるべく単純に使えるようにしてありま
す。一般的な用途ではこの API を使用してください。

"下位API"は C 言語で書かれたライブラリです。OCI((-Oracle Call
Interface: オラクルの C言語インターフェース-))のハンドルを ruby のクラ
スに、OCI の関数を ruby のメソッドへマッピングしてあります。ruby と C
言語の言語仕様の違いにより、単純なマッピングができないところもあります
が、可能なかぎり元の API を変えないようにしてあります。

バージョン 0.2 では C 言語でもって上位APIを直接書き直し、下位APIはなく
なる予定です。

== 目次
* ((<クラス一覧>))
  * ((<OCI8>))
  * ((<OCI8::Cursor>))
  * ((<OCI8::BLOB>))
  * ((<OCI例外クラス>))
* ((<メソッド一覧>))
  * OCI8
    * ((<new|OCI8.new>))(userid, password, dbname = nil, privilege = nil)
    * ((<logoff|OCI8#logoff>))()
    * ((<exec|OCI8#exec>))(sql, *bindvars)
    * ((<parse|OCI8#parse>))(sql)
    * ((<commit|OCI8#commit>))()
    * ((<rollback|OCI8#rollback>))()
    * ((<autocommit?|OCI8#autocommit?>))
    * ((<autocommit|OCI8#autocommit>))
    * ((<autocommit=|OCI8#autocommit=>))
    * ((<non_blocking?|OCI8#non_blocking?>))
    * ((<non_blocking=|OCI8#non_blocking=>))
    * ((<break|OCI8#break>))()
  * OCI8::Cursor
    * ((<define|OCI8::Cursor#define>))(pos, type, length = nil)
    * ((<bind_param|OCI8::Cursor#bind_param>))(key, val, type = nil, length = nil)
    * ((<[]|OCI8::Cursor#[]>))(key)
    * ((<[]=|OCI8::Cursor#[]=>))(key, val)
    * ((<keys|OCI8::Cursor#keys>))()
    * ((<exec|OCI8::Cursor#exec>))(*bindvars)
    * ((<type|OCI8::Cursor#type>))
    * ((<row_count|OCI8::Cursor#row_count>))
    * ((<get_col_names|OCI8::Cursor#get_col_names>))
    * ((<getColNames|OCI8::Cursor#getColNames>))
    * ((<fetch|OCI8::Cursor#fetch>))()
    * ((<close|OCI8::Cursor#close>))()
    * ((<rowid|OCI8::Cursor#rowid>))
  * OCI8::BLOB
    * ((<available?|OCI8::BLOB#available?>))
    * ((<read|OCI8::BLOB#read>))(size = nil)
    * ((<write|OCI8::BLOB#write>))(data)
    * ((<size|OCI8::BLOB#size>))
    * ((<size=|OCI8::BLOB#size=>))(len)
    * ((<chunk_size|OCI8::BLOB#chunk_size>))
    * ((<truncate|OCI8::BLOB#truncate>))(len)
    * ((<pos|OCI8::BLOB#pos>))
    * ((<pos=|OCI8::BLOB#pos=>))(pos)
    * ((<tell|OCI8::BLOB#tell>))
    * ((<seek|OCI8::BLOB#seek>))(pos)
    * ((<rewind|OCI8::BLOB#rewind>))
    * ((<eof?|OCI8::BLOB#eof?>))
* ((<補遺>))
  * ((<"ブロッキング/非ブロッキングモード">))
== クラス一覧
上位APIで必須なクラスは、((<OCI8>)), ((<OCI8::Cursor>)), ((<OCI8::BLOB>)),
および((<OCI例外クラス>))です。

=== OCI8
このクラスのインスタンスはデータベースへの接続に対応します。JDBC の
java.sql.Connection, Perl/DBI の database handle: $dbh に対応します。

単純な SQL の実行ならば、このクラスのみで実行できます。

=== OCI8::Cursor
このクラスのインスタンスはオラクルの用語ではカーソルに対応します。JDBC
の java.sql.Statement, Perl/DBI の statement handle: $sth に対応します。

このクラスのインスタンスは new により生成しないでください。必ず
((<OCI8#exec>)), ((<OCI8#parse>)) 経由で生成してください。

=== OCI8::BLOB
BLOB のバイナリデータを読み書きするときに使用するクラスです。
select 文で BLOB 型のカラムを選択するとこのクラスのインスタンスが自動
的に生成されます。

=== OCI例外クラス
上位APIで必要な例外クラスの階層は以下のようになっています。

* ((|OCIException|))
  * ((|OCIError|))
  * ((|OCIInvalidHandle|))
  * ((|OCIBreak|))

((|OCIException|))は OCI 例外の抽象親クラスです。OCI の例外をすべて補
足したいときはこのクラスを rescue に使ってください。

((|OCIError|))はオラクルのエラーコードつきの例外クラスです。
OCIError#message でメッセージの文字列を、OCIErrror#code でエラーコード
を取得できます。

((|OCIInvalidHandle|))は無効なハンドルに対して操作を行なったときに起こ
る例外です。

((|OCIBreak|))は((<非ブロッキングモード|"ブロッキング/非ブロッキングモード">))
で別のスレッドより OCI 呼出しがキャンセルされたときに起こる例外です。

== メソッド一覧
=== OCI8
--- OCI8.new(userid, password, dbname = nil, privilege = nil)
     ((|userid|)), ((|password|)) でもってオラクルへ接続します。((|dbname|)) は Net8 の
     接続文字列です。DBA 権限が必要な場合は ((|privilege|)) に :SYSDBA また
     は :SYSOPERを指定します。


     例:
       # sqlplus scott/tiger@orcl.world
       conn = OCI8.new("scott", "tiger", "orcl.world")

     例:
       # sqlplus 'sys/change_on_install as sysdba'
       conn = OCI8.new("sys", "change_on_install", nil, :SYSDBA)

--- OCI8#logoff()
     オラクルとの接続を切ります。コミットされてないトランザクションは
     ロールバックされます。

     例:
       conn = OCI8.new("scott", "tiger")
       ... do something ...
       conn.logoff

--- OCI8#exec(sql, *bindvars)
     sql 文を実行します。sql 文が SELECT文、INSERT/UPDATE/DELETE文、
     CREATE/ALTER/DROP文、PL/SQL文、それぞれ場合で戻り値の種類が異なり
     ます。

     bindvars がある場合、バインド変数としてバインドしてから実行します。

     SELECT 文でブロックがついてない場合、OCI8::Cursor のインスタンス
     を返します。

     例:
       conn = OCI8.new('scott', 'tiger')
       cursor = conn.exec('SELECT * FROM emp')
       while r = cursor.fetch()
         puts r.join(',')
       end
       cursor.close
       conn.logoff

     SELECT 文でブロックがついている場合、イテレータとして動き、処理
     した行数が返ります。ブロックにはフェッチしたデータが配列でわたり
     ます。NULL値は ruby 側では nil にマッピングしてあります。


     例:
       conn = OCI8.new('scott', 'tiger')
       num_rows = conn.exec('SELECT * FROM emp') do |r|
         puts r.join(',')
       end
       puts num_rows.to_s + ' rows were processed.'
       conn.logoff

     INSERT/UPDATE/DELETE文の場合、それぞれ処理した行数が返ります。

     例:
       conn = OCI8.new('scott', 'tiger')
       num_rows = conn.exec('UPDATE emp SET sal = sal * 1.1')
       puts num_rows.to_s + ' rows were updated.'
       conn.logoff

     CREATE/ALTER/DROP文の場合、true が返ります。

     例:
       conn = OCI8.new('scott', 'tiger')
       conn.exec('CREATE TABLE test (col1 CHAR(6))')
       conn.logoff

     PL/SQL文の場合、実行後のバインド変数の値が配列となって返ります。

     例:
       conn = OCI8.new('scott', 'tiger')
       conn.exec("BEGIN :str := TO_CHAR(:num, 'FM0999'); END;", 'ABCD', 123)
       # => ["0123", 123]
       conn.logoff

     上記の例では、((|:str|)) と ((|:num|)) という2つのバインド変数が
     あります。初期値としてそれぞれ"幅4で値は ABCD の文字列"と"値 123
     の数値"が設定されてから、PL/SQL文が実行され、実行後のバインド変数
     の値が配列として返ってきます。配列の順番はバインド変数の順番と同
     じです。

--- OCI8#parse(sql)
     カーソルを作成し、sql 文実行の準備をします。OCI8::Cursor のインス
     タンスが返ります。

--- OCI8#commit()
     トランザクションをコミットします。

     例:
       conn = OCI8.new("scott", "tiger")
       conn.exec("UPDATE emp SET sal = sal * 1.1") # yahoo
       conn.commit
       conn.logoff

--- OCI8#rollback()
     トランザクションをロールバックします。

     例:
       conn = OCI8.new("scott", "tiger")
       conn.exec("UPDATE emp SET sal = sal * 0.9") # boos
       conn.rollback
       conn.logoff

--- OCI8#autocommit?
     autocommit モードの状態を返します。デフォルトは false です。この
     値が true のとき、INSERT/UPDATE/DELETE文が実行される毎に自動的に
     コミットされます。

--- OCI8#autocommit
     ((<OCI8#autocommit?>))の別名です。

--- OCI8#autocommit=
     autocommit モードの状態を変更します。true か flase を設定してください。

     例:
       conn = OCI8.new("scott", "tiger")
       conn.autocommit = true
       ... do something ...
       conn.logoff

--- OCI8#non_blocking?
     ブロッキング/非ブロッキングモードの状態を返します。デフォルトは
     false、つまりブロッキングモードです。((<"ブロッキング/非ブロッキングモード">))
     を参照してください。


--- OCI8#non_blocking=
     ブロッキング/非ブロッキングモードの状態を変更します。true か
     flase を設定してください。((<"ブロッキング/非ブロッキングモード">))
     を参照してください。


--- OCI8#break()
     実行中の他スレッドの OCI 呼出しをキャンセルします。このメソッドを
     実行するには非ブロッキングモードである必要があります。
     ((<"ブロッキング/非ブロッキングモード">))を参照してください。

== OCI8::Cursor
--- OCI8::Cursor#define(pos, type, length = nil)

     fetch で取得するデータの型を明示的に指定する。parse と exec の間
     に実行してください。pos は 1 から数えます。length は type に
     String を指定したときに有効です。

     例:
       cursor = conn.parse("SELECT ename, hiredate FROM emp")
       cursor.define(1, String, 20) # 1カラム目を String として fetch
       cursor.define(2, Time)       # 2カラム目を Time として fetch
       cursor.exec()

--- OCI8::Cursor#bind_param(key, val, type = nil, length = nil)
     明示的に変数をバインドします。

     key が数値の場合は、バインド変数の位置によってバインドします。位
     置は1から数えます。key が文字列の場合は、バインド変数の名前によっ
     てバインドします。

     例:
       cursor = conn.parse("SELECT * FROM emp WHERE ename = :ename")
       cursor.bind_param(1, 'SMITH') # bind by position
         ...or...
       cursor.bind_param(':ename', 'SMITH') # bind by name

     数値をバインドする場合、Fixnum か Float が使ってください。Bignum
     は使用できません。初期値を NULL にする場合は、val を nil にして、
     type をFixnum か Float にしてください。

     例:
       cursor.bind_param(1, 1234) # bind as Fixnum, Initial value is 1234.
       cursor.bind_param(1, 1234.0) # bind as Float, Initial value is 1234.0.
       cursor.bind_param(1, nil, Fixnum) # bind as Fixnum, Initial value is NULL.
       cursor.bind_param(1, nil, Float) # bind as Float, Initial value is NULL.

     文字列をバインドする場合、val にそのままバインドする文字を指定し
     てください。バインドが出力として使われる場合、出力するのに必要な
     長さの文字を指定するか、type に String を指定した上で length を指
     定してください。

     例:
       cursor = conn.parse("BEGIN :out := :in || '_OUT'; END;")
       cursor.bind_param(':in', 'DATA') # bind as String with width 4.
       cursor.bind_param(':out', nil, String, 7) # bind as String with width 7.
       cursor.exec()
       p cursor[':out'] # => 'DATA_OU'
       # PL/SQLブロック内での :out は 8バイトだが、幅7の文字としてバインド
       # してあるので、7バイト目で切れている

     文字列を RAW 型としてバインドする場合は必ず type に OCI8::RAW を
     指定してください。

     例:
       cursor = conn.parse("INSERT INTO raw_table(raw_column) VALUE (:1)")
       cursor.bind_param(1, 'RAW_STRING', OCI8::RAW)
       cursor.exec()
       cursor.close()

--- OCI8::Cursor#[](key)
     バインド変数の値を取り出します。

     明示的にバインドした場合、((<OCI8::Cursor#bind_param>))で指定した
     key でもって取り出します。位置によるバインド、名前によるバインド
     が混在した場合、同じ場所でも指定したバインド方法に対応した key を
     用いてください。

     例:
       cursor = conn.parse("BEGIN :out := 'BAR'; END;")
       cursor.bind_param(':out', 'FOO') # bind by name
       p cursor[':out'] # => 'FOO'
       p cursor[1] # => nil
       cursor.exec()
       p cursor[':out'] # => 'BAR'
       p cursor[1] # => nil

     例:
       cursor = conn.parse("BEGIN :out := 'BAR'; END;")
       cursor.bind_param(1, 'FOO') # bind by position
       p cursor[':out'] # => nil
       p cursor[1] # => 'FOO'
       cursor.exec()
       p cursor[':out'] # => nil
       p cursor[1] # => 'BAR'

     ((<OCI8#exec>))、((<OCI8::Cursor#exec>))を使用してバインドした場
     合、バインド変数の位置でもって取得します。

     例:
       cursor = conn.exec("BEGIN :out := 'BAR'; END;", 'FOO')
       # 1st bind variable is bound as String with width 3. Its initial value is 'FOO'
       # After execute, the value become 'BAR'.
       p cursor[1] # => 'BAR'

--- OCI8::Cursor#[]=(key, val)
     バインド変数に値を設定します。key の指定方法は
     ((<OCI8::Cursor#[]>))と同じです。((<OCI8::Cursor#bind_param>))で指
     定した val を後で置き換えたいとき、また、別の値に置き換えて何度も
     ((<OCI8::Cursor#exec>))を実行したいときに使用してください。

     例１:
       cursor = conn.parse("INSERT INTO test(col1) VALUES(:1)")
       cursor.bind_params(1, nil, String, 3)
       ['FOO', 'BAR', 'BAZ'].each do |key|
         cursor[1] = key
         cursor.exec
       end
       cursor.close()

     例２:
       ['FOO', 'BAR', 'BAZ'].each do |key|
         conn.exec("INSERT INTO test(col1) VALUES(:1)", key)
       end

     例１と例２は結果は同じですが、例１のほうが負荷が少なくなります。

--- OCI8::Cursor#keys()
     バインド変数の key を配列にして返します。

--- OCI8::Cursor#exec(*bindvars)
     カーソルに割当てられた SQL を実行します。SQL が SELECT文、
     INSERT/UPDATE/DELETE文、CREATE/ALTER/DROP文とPL/SQL文、それぞれ場
     合で戻り値の種類が異なります。


     SELECT文の場合、select-list の数が返ります。

     INSERT/UPDATE/DELETE文の場合、それぞれ処理した行数が返ります。

     CREATE/ALTER/DROP文とPL/SQL文の場合、true が返ります。
     ((<OCI8#exec>))では PL/SQL文の場合、バインド変数の値が配列となって
     返りますが、このメソッドでは単に true が返ります。バインド変数の
     値は((<OCI8::Cursor#[]>))でもって明示的に取得してください。

--- OCI8::Cursor#type
     SQL文の種類を取得します。戻り値は、
     * OCI8::STMT_SELECT
     * OCI8::STMT_UPDATE
     * OCI8::STMT_DELETE
     * OCI8::STMT_INSERT
     * OCI8::STMT_CREATE
     * OCI8::STMT_DROP
     * OCI8::STMT_ALTER
     * OCI8::STMT_BEGIN
     * OCI8::STMT_DECLARE
     のどれかです。PL/SQL文の場合は、OCI8::STMT_BEGIN か
     OCI8::STMT_DECLARE となります。

--- OCI8::Cursor#row_count
     処理した行数を返します。

--- OCI8::Cursor#get_col_names
     選択リストの名前を配列で取得します。SELECT 文にのみ有効です。必ず exec した後に使用してください。

--- OCI8::Cursor#getColNames
     ((<OCI8::Cursor#get_col_names>)) へのエイリアスです。

--- OCI8::Cursor#fetch()
     SELECT 文のみに有効です。フェッチしたデータが配列として返ります。

     例:
       conn = OCI8.new('scott', 'tiger')
       cursor = conn.exec('SELECT * FROM emp')
       while r = cursor.fetch()
         puts r.join(',')
       end
       cursor.close
       conn.logoff

--- OCI8::Cursor#close()
     カーソルをクローズします。

--- OCI8::Cursor#rowid
     現在実行している行の ROWID を取得します。
     ここで得られた値はバインド値として使用できます。
     逆に言うと、バインドする目的にしか使用できません。

== OCI8::BLOB
--- OCI8::BLOB#available?
     BLOB が有効かどうかチェックします。
     BLOB を使用するためには最初に空のBLOBを挿入する必要があります。

     例:
       conn.exec("CREATE TABLE photo (name VARCHAR2(50), image BLOB)")
       conn.exec("INSERT INTO photo VALUES ('null-data', NULL)")
       conn.exec("INSERT INTO photo VALUES ('empty-data', EMPTY_BLOB())")
       conn.exec("SELECT name, image FROM photo") do |name, image|
         case name
         when 'null-data'
           puts "#{name} => #{image.available?.to_s}"
           # => false
         when 'empty-data'
           puts "#{name} => #{image.available?.to_s}"
           # => true
         end
       end

--- OCI8::BLOB#read(size = nil)
     BLOB よりデータを読み込みます。size の指定がない場合はデータの最
     後まで読み込みます。

     例１: BLOB のチャンクサイズ毎に読み込み。
       conn.exec("SELECT name, image FROM photo") do |name, image|
         chunk_size = image.chunk_size
         File.open(name, 'w') do |f|
           until image.eof?
             f.write(image.read(chunk_size))
           end
         end
       end

     例２: 一挙に読み込み。
       conn.exec("SELECT name, image FROM photo") do |name, image|
         File.open(name, 'w') do |f|
           f.write(image.read)
         end
       end

--- OCI8::BLOB#write(data)
     BLOB へデータを書き込みます。
     BLOB に元々入っていたデータの長さが書き込んだデータより長い場合は
     ((<OCI8::BLOB#size=>)) で長さの再設定を行ってください。

     例１: BLOB のチャンクサイズ毎に書き込み
       cursor = conn.parse("INSERT INTO photo VALUES(:name, EMPTY_BLOB())")
       Dir["*.png"].each do |fname|
         cursor.exec(fname)
       end
       conn.exec("SELECT name, image FROM photo") do |name, image|
         chunk_size = image.chunk_size
         File.open(name, 'r') do |f|
           until f.eof?
             image.write(f.read(chunk_size))
           end
           image.size = f.pos
         end
       end
       conn.commit

     例２: 一挙に書き込み
       conn.exec("SELECT name, image FROM photo") do |name, image|
         File.open(name, 'r') do |f|
           image.write(f.read)
           image.size = f.pos
         end
       end

--- OCI8::BLOB#size
     BLOB のデータの長さを返します。

--- OCI8::BLOB#size=(len)
     BLOB のデータの長さを len に設定します。

--- OCI8::BLOB#chunk_size
     BLOB のチャンクサイズを返します。

--- OCI8::BLOB#truncate(len)
     BLOB のデータの長さを len に設定します。

--- OCI8::BLOB#pos
     次回の read/write の開始位置を返します。

--- OCI8::BLOB#pos=(pos)
     次回の read/write の開始位置を設定します。

--- OCI8::BLOB#eof?
     BLOB の最後に到達したかどうかを返します。

--- OCI8::BLOB#tell
     ((<OCI8::BLOB#pos>)) と同じです。

--- OCI8::BLOB#seek(pos)
     ((<OCI8::BLOB#pos=>)) と同じです。

--- OCI8::BLOB#rewind
     次回の read/write の開始位置を 0 に設定します。

== 補遺
=== ブロッキング/非ブロッキングモード
デフォルトはブロッキングモードになっています。((<OCI8#non_blocking=>))
でモードを変更できます。

ブロッキングモードの場合、重い OCI の呼出しをしていると、スレッドを使っ
ていても ruby 全体がブロックされます。それは、ruby のスレッドはネイティ
ブスレッドではないからです。

非ブロッキングモードの場合、重い OCI の呼出しでも ruby 全体はブロック
されません。OCI を呼出しているスレッドのみがブロックします。そのかわり、
OCI 呼出しが終了しているかどうかポーリングによってチェックしているので、
個々の OCI 呼出しが少し遅くなります。

((<OCI8#break>)) を用いると、別のスレッドから重い OCI 呼出しをキャンセルでき
ます。キャンセルされたスレッドでは ((|OCIBreak|)) という例外を挙げます。

非ブロッキングモードの制限事項: 別々のスレッドで同じ接続に対して同時に
OCI 呼出しを行わないでください。OCI ライブラリ側が非ブロッキングモード
での交互の呼出しに対応しているかどうか不明ですし、また、ruby のモジュー
ル側でも、((<OCI8#break>)) を実行したときにキャンセルするOCI 呼出しが
複数ある場合、現在の実装では対応できません。

=end
