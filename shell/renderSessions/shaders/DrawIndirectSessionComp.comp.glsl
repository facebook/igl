#version SparkSL 3.0.0
layout(local_size_x=3, local_size_y=1) in;
layout(set=1, binding=0) buffer indexCountBuffer {
  uint indexCountBufferValue;
};

void main() {
  atomicAdd(indexCountBufferValue, 1u);
}
