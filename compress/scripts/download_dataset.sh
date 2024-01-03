if [ ! -d "../data" ]; then
  mkdir "../data"
fi

## scientific simulation dataset
wget http://www.cs.txstate.edu/~burtscher/research/datasets/FPdouble/msg_bt.trace.fpc -o ../data/msg_bt_f64
wget http://www.cs.txstate.edu/~burtscher/research/datasets/FPdouble/num_brain.trace.fpc -o ../data/num_brain_f64
wget http://www.cs.txstate.edu/~burtscher/research/datasets/FPdouble/num_control.trace.fpc -o ../data/num_control_f64
wget https://klacansky.com/open-scivis-datasets/tacc_turbulence/tacc_turbulence_256x256x256_float32.raw -o ../data/turbulence_f32

## time series dataset
