if [ ! -d "./data-old" ]; then
  mkdir "./data-old"
fi

## scientific simulation data-oldset
wget http://www.cs.txstate.edu/~burtscher/research/data-oldsets/FPdouble/msg_bt.trace.fpc -O ./data-old/msg_bt_f64
wget http://www.cs.txstate.edu/~burtscher/research/data-oldsets/FPdouble/num_brain.trace.fpc -O ./data-old/num_brain_f64
wget http://www.cs.txstate.edu/~burtscher/research/data-oldsets/FPdouble/num_control.trace.fpc -O ./data-old/num_control_f64
wget https://klacansky.com/open-scivis-data-oldsets/tacc_turbulence/tacc_turbulence_256x256x256_float32.raw -O ./data-old/turbulence_f32

## time series data-oldset
