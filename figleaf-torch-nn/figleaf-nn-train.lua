----------------------------------------------------------------------
-- This is a face recognition neural network specifically designed to break
-- image obfuscation via blurring, encryption, etc. The script itself is
-- adapted from the Torch MNIST digit classifier originally written by Clement
-- Farabet. The neural network is one proposed by Richard McPherson, Reza
-- Shokri, and Vitaly Shmatikov in their 2016 paper titled "Defeating Image
-- Obfuscation with Deep Learning".
--
-- At present, this tool works only for the AT&T Faces Dataset.
--
-- This script written by Byron Marohn for a 2017 independent study with Dr.
-- Charles Wright at Portland State University. Its purpose is to determine how
-- well the figleaf thumbnail preserving encryption schemes are able to defeat
-- neural network face classifiers.
--
----------------------------------------------------------------------

require 'torch'
require 'nn'
require 'nnx'
require 'optim'
require 'image'
require 'figleaf-nn-dataset'
require 'pl'
require 'paths'

----------------------------------------------------------------------
-- parse command-line options
--
local opt = lapp[[
   -s,--save          (default "logs")      subdirectory to save logs
   -n,--network       (default "")          reload pretrained network
   -p,--plot                                plot while training
   -l,--load          (default "")          path to load image data from
   -r,--learningRate  (default 0.01)        learning rate
   -b,--batchSize     (default 10)          batch size
   -m,--momentum      (default 0.9)         momentum
   -p,--type          (default cuda)        float or cuda
   -i,--devid         (default 1)           device ID (if using CUDA)
   -t,--threads       (default 4)           number of threads
   -e,--maxEpoch      (default 500)         maximum number of epochs
   -x,--splitIdx      (default 1)           train/test split table index to use
]]

if opt.load == '' then
   print('Use --load to specify a directory tree containing input images')
   print('Each person (set of faces) should be in its own subdirectory with one or more samples')
   return
end

-- fix seed
torch.manualSeed(1)

-- threads
torch.setnumthreads(opt.threads)
print('<torch> set nb of threads to ' .. torch.getnumthreads())

-- type:
if opt.type == 'cuda' then
   print('==> switching to CUDA')
   require 'cunn'
   cutorch.setDevice(opt.devid)
   print('==> using GPU #' .. cutorch.getDevice())
   nn.SpatialConvolutionMM = nn.SpatialConvolution
end

----------------------------------------------------------------------
-- Load and normalize training and testing datasets
trainData, testData, geometry = figleaf.loadDataset(opt.load, opt.splitIdx)
trainData:normalizeGlobal(mean, std)
testData:normalizeGlobal(mean, std)

----------------------------------------------------------------------
-- define model to train
-- on the 10-class classification problem
--
classes = {}
for x=1,trainData:numClasses() do
   table.insert(classes, tostring(x))
end

if opt.network == '' then
   -- define model to train
   model = nn.Sequential()

   -- (1)
   -- Arg 1   - Take elements from a 1-dimensional input plane
   -- Arg 2   - 32 output "feature map" signals PER CONVOLVED INPUT PIXEL
   -- Arg 3,4 - Convolve them over a 3 x 3 window
   -- Arg 5,6 - Do this for every pixel (step size = 1 in x, 1 in y)
   -- Arg 7,8 - Pad one additional zero element on each side of each axis.
   --    This effectively makes the images 94 x 114, with the outermost box zeros
   --
   -- The output will be 32 "feature map" layers of size 92 x 112
   model:add(nn.SpatialConvolutionMM(1, 32, 3, 3, 1, 1, 1, 1))

   -- (2)
   -- Non-linearity function
   -- Passes through input if input >= 0, otherwise 0.01 * input if input < 0
   model:add(nn.LeakyReLU(0.01))

   -- (3)
   -- Apply 2D max-pooling to 2x2 windows every 2 pixels
   -- This reduces the images from size 92 x 112 to 46 x 56
   -- Basically "MaxPooling" is just returning the maximum of the 2x2 window
   -- This layer has no trainable weights - just a max
   -- This apparently reduces the search space dramatically
   --    (makes sense, 1/4 the inputs to the next layer)
   model:add(nn.SpatialMaxPooling(2, 2, 2, 2))

   -- (4)
   -- Arg 1   - Take elements from each of the 32 input feature maps
   -- Arg 2   - 64 output "feature map" signals PER CONVOLVED INPUT PIXEL across all input feature maps
   -- Arg 3,4 - Convolve them over a 3 x 3 window
   -- Arg 5,6 - Do this for every pixel (step size = 1 in x, 1 in y)
   -- Arg 7,8 - Pad one additional zero element on each side of each axis.
   --    This effectively makes the images 48 x 58, with the outermost box zeros
   --
   -- The output will be 64 "feature map" layers of size 46 x 56
   model:add(nn.SpatialConvolutionMM(32, 64, 3, 3, 1, 1, 1, 1))

   -- (5)
   model:add(nn.LeakyReLU(0.01))

   -- (6)
   -- Output size reduced again, still 64 layers but only 23 x 28 per layer
   model:add(nn.SpatialMaxPooling(2, 2, 2, 2))

   -- (7)
   -- Arg 1   - Take elements from each of the 64 input feature maps
   -- Arg 2   - 128 output "feature map" signals PER CONVOLVED INPUT PIXEL across all input feature maps
   -- Arg 3,4 - Convolve them over a 3 x 3 window
   -- Arg 5,6 - Do this for every pixel (step size = 1 in x, 1 in y)
   -- Arg 7,8 - Pad one additional zero element on each side of each axis.
   --    This effectively makes the images 25 x 30, with the outermost box zeros
   --
   -- The output will be 128 "feature map" layers of size 23 x 28
   model:add(nn.SpatialConvolutionMM(64, 128, 3, 3, 1, 1, 1, 1))

   -- (8)
   model:add(nn.LeakyReLU(0.01))

   -- (9)
   -- Output size reduced again, still 128 layers but only 7 x 10 per layer
   -- Edge values must be discarded somehow?
   model:add(nn.SpatialMaxPooling(3, 3, 3, 3))

   -- (10)
   -- Flatten the data into a giant array of size 128 * 7 * 10 = 8064
   model:add(nn.Reshape(8064))

   -- (11)
   -- Fully connected layer (apparently matrix multiplication?)
   -- Maps 8064 input to 1024 output
   model:add(nn.Linear(8064, 1024))

   -- (12)
   model:add(nn.LeakyReLU(0.01))

   -- (13)
   -- Randomly drop out (set output to zero) neurons in the network
   -- This apparently helps regularize the data during training
   -- Apparently when using the model to evaluate things, this layer
   -- simply passes the input through without modification
   model:add(nn.Dropout(0.5))

   -- (14)
   -- Again, use a matrix to condense the previous output layers into
   -- 40 possible results
   model:add(nn.Linear(1024, trainData:numClasses()))

   -- (15)
   -- Converts output to log-probability for each potential choice
   model:add(nn.LogSoftMax())

else
   print('<trainer> reloading previously trained network')
   model = torch.load(opt.network)
end


-- verbose
print('<figleaf> using model:')
print(model)

----------------------------------------------------------------------
-- loss function: negative log-likelihood
--
criterion = nn.ClassNLLCriterion()

if opt.type == 'cuda' then
   model:cuda()
   criterion:cuda()
end

-- retrieve parameters and gradients
parameters,gradParameters = model:getParameters()

----------------------------------------------------------------------
-- define training and testing functions
--

-- this matrix records the current confusion across classes
confusion = optim.ConfusionMatrix(classes)

-- log results to files
trainLogger = optim.Logger(paths.concat(opt.save, 'train.log'))
testLogger = optim.Logger(paths.concat(opt.save, 'test.log'))

-- TODO: Need more flexibility in batchSize so it works with uneven number of images
-- Right now strange things will happen if the number of test images isn't a
-- multiple of the batchSize.
if math.fmod(trainData.size(), opt.batchSize) ~= 0 or math.fmod(testData.size(), opt.batchSize) ~= 0 then
   print('Error: Number of train and test images must be divisible by batchSize')
   return
end

local inputs = torch.Tensor(opt.batchSize, 1, geometry[1], geometry[2])
local targets = torch.Tensor(opt.batchSize)

if opt.type == 'cuda' then
   inputs = inputs:cuda()
   targets = targets:cuda()
end

-- training function
function train(dataset, epoch)
   -- local vars
   local time = sys.clock()

   -- do one epoch
   print('<trainer> on training set:')
   print("<trainer> online epoch # " .. epoch .. ' [batchSize = ' .. opt.batchSize .. ']')
   for t = 1,dataset:size(),opt.batchSize do
      -- create mini batch
      local k = 1
      for i = t,math.min(t+opt.batchSize-1,dataset:size()) do
         inputs[k] = dataset.data[i]
         targets[k] = dataset.labels[i]
         k = k + 1
      end

      -- create closure to evaluate f(X) and df/dX
      local feval = function(x)
         -- just in case:
         collectgarbage()

         -- get new parameters
         if x ~= parameters then
            parameters:copy(x)
         end

         -- reset gradients
         gradParameters:zero()

         -- evaluate function for complete mini batch
         local outputs = model:forward(inputs)
         local f = criterion:forward(outputs, targets)

         -- estimate df/dW
         local df_do = criterion:backward(outputs, targets)
         model:backward(inputs, df_do)

         -- update confusion
         for i = 1,opt.batchSize do
            confusion:add(outputs[i], targets[i])
         end

         -- return f and df/dX
         return f,gradParameters
      end

      -- Perform SGD step:
      sgdState = sgdState or {
         learningRate = opt.learningRate,
         momentum = opt.momentum,
         learningRateDecay = 1e-7
      }
      optim.sgd(feval, parameters, sgdState)

      -- disp progress
      xlua.progress(t, dataset:size())
   end

   -- time taken
   time = sys.clock() - time
   time = time / dataset:size()
   print("<trainer> time to learn 1 sample = " .. (time*1000) .. 'ms')

   -- print confusion matrix
   print(confusion)
   trainLogger:add{['% mean class accuracy (train set)'] = confusion.totalValid * 100}
   confusion:zero()

   -- save/log current net
   --local filename = paths.concat(opt.save, 'figleaf.net')
   --os.execute('mkdir -p ' .. sys.dirname(filename))
   --if paths.filep(filename) then
      --os.execute('mv ' .. filename .. ' ' .. filename .. '.old')
   --end
   --print('<trainer> saving network to '..filename)
   -- torch.save(filename, model)
end

-- test function
function test(dataset)
   -- local vars
   local time = sys.clock()

   -- test over given dataset
   print('<trainer> on testing Set:')
   for t = 1,dataset:size(),opt.batchSize do
      -- disp progress
      xlua.progress(t, dataset:size())

      -- create mini batch
      local k = 1
      for i = t,math.min(t+opt.batchSize-1,dataset:size()) do
         inputs[k] = dataset.data[i]
         targets[k] = dataset.labels[i]
         k = k + 1
      end

      -- test samples
      local preds = model:forward(inputs)

      -- confusion:
      for i = 1,opt.batchSize do
         confusion:add(preds[i], targets[i])
      end
   end

   -- timing
   time = sys.clock() - time
   time = time / dataset:size()
   print("<trainer> time to test 1 sample = " .. (time*1000) .. 'ms')

   -- print confusion matrix
   print(confusion)
   testLogger:add{['% mean class accuracy (test set)'] = confusion.totalValid * 100}
   confusion:zero()
end

----------------------------------------------------------------------
-- and train!
--
epoch = 1
while epoch <= opt.maxEpoch do

   -- train/test
   train(trainData, epoch)
   test(testData)

   -- plot errors
   if opt.plot then
      trainLogger:style{['% mean class accuracy (train set)'] = '-'}
      testLogger:style{['% mean class accuracy (test set)'] = '-'}
      trainLogger:plot()
      testLogger:plot()
   end

   -- next epoch
   epoch = epoch + 1
end
