# RUN  -  rspec spec/main_spec.rb

describe 'database' do
  before do
    `rm -rf test.db`
  end

  def run_script(commands)
    raw_output = nil
    IO.popen("./main test.db", "r+") do |pipe|
      commands.each do |command|
        pipe.puts command
      end

      pipe.close_write

      # Read entire output
      raw_output = pipe.gets(nil)
    end
    raw_output.split("\n")
  end

  it 'insert and retrieves a row' do
    result = run_script([
      "insert 1 ema1 ema@ema.ema",
      "select",
      ".exit",
    ])
    expect(result).to match_array([
      "Ema's db > Executed",
      "Ema's db > (1, ema1, ema@ema.ema)",
      "Executed",
      "Ema's db > ",
    ])
  end

  it 'prints error message when table is full' do
    script = (1..1401).map do |i|
      "insert #{i} ema#{i} ema#{i}@ema.ema"
    end
    script << ".exit"
    result = run_script(script)
    expect(result[-2]).to eq("Ema's db > Error: Table full")
  end

  it 'allows inserting strings that are the maximum length' do
    long_username = "e"*32
    long_email = "e"*255
    script = [
      "insert 1 #{long_username} #{long_email}",
      "select",
      ".exit",
    ]
    result = run_script(script)
    expect(result).to match_array([
      "Ema's db > Executed",
      "Ema's db > (1, #{long_username}, #{long_email})",
      "Executed",
      "Ema's db > ",
    ])
  end

  it 'prints error message if strings are too long' do
    long_username = "e"*33
    long_email = "e"*256
    script = [
      "insert 1 #{long_username} #{long_email}",
      "select",
      ".exit",
    ]
    result = run_script(script)
    expect(result).to match_array([
      "Ema's db > String is too long",
      "Ema's db > Executed",
      "Ema's db > ",
    ])
  end

  it 'prints an error message if id is negative' do
    script = [
      "insert -1 ema ema@ema.ema",
      "select",
      ".exit",
    ]
    result = run_script(script)
    expect(result).to match_array([
      "Ema's db > ID must be positive",
      "Ema's db > Executed",
      "Ema's db > "
    ])
  end

  it 'keeps data after closing connetion' do
    result1 = run_script([
      "insert 1 ema ema@ema.ema",
      ".exit",
    ])
    expect(result1).to match_array([
      "Ema's db > Executed",
      "Ema's db > ",
    ])
    result2 = run_script([
      "select",
      ".exit"
    ])
    expect(result2).to match_array([
      "Ema's db > (1, ema, ema@ema.ema)",
      "Executed",
      "Ema's db > "
    ])
  end
end
