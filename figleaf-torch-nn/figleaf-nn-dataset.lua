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
require 'paths'
require 'lfs'
require 'image'

figleaf = {}

function figleaf.loadDataset(filePath, splitIdx)
   -- Make sure filePath has a trailing slash
   filePath = filePath .. '/'

   -- These are the hardcoded pseudorandom test/train splits
   local trainTestSplit =
      {
         [1] = {[1] = "train", [2] = "train", [3] = "train", [4] = "train", [5] = "train", [6] = "train", [7] = "train", [8] = "test", [9] = "test", [10] = "test"},
         [2] = {[1] = "train", [2] = "train", [3] = "test", [4] = "test", [5] = "train", [6] = "test", [7] = "train", [8] = "train", [9] = "train", [10] = "train"},
         [3] = {[1] = "test", [2] = "train", [3] = "test", [4] = "train", [5] = "train", [6] = "test", [7] = "train", [8] = "train", [9] = "train", [10] = "train"},
         [4] = {[1] = "test", [2] = "train", [3] = "train", [4] = "train", [5] = "train", [6] = "train", [7] = "test", [8] = "test", [9] = "train", [10] = "train"},
         [5] = {[1] = "train", [2] = "train", [3] = "train", [4] = "train", [5] = "train", [6] = "test", [7] = "test", [8] = "train", [9] = "test", [10] = "train"},
         [6] = {[1] = "test", [2] = "train", [3] = "train", [4] = "train", [5] = "test", [6] = "train", [7] = "train", [8] = "train", [9] = "train", [10] = "test"},
         [7] = {[1] = "train", [2] = "test", [3] = "train", [4] = "train", [5] = "train", [6] = "train", [7] = "train", [8] = "train", [9] = "test", [10] = "test"},
         [8] = {[1] = "train", [2] = "train", [3] = "train", [4] = "test", [5] = "train", [6] = "train", [7] = "test", [8] = "test", [9] = "train", [10] = "train"},
         [9] = {[1] = "train", [2] = "test", [3] = "train", [4] = "test", [5] = "test", [6] = "train", [7] = "train", [8] = "train", [9] = "train", [10] = "train"},
         [10] = {[1] = "train", [2] = "test", [3] = "test", [4] = "train", [5] = "test", [6] = "train", [7] = "train", [8] = "train", [9] = "train", [10] = "train"}
      }

   -- TODO: Autodetect from input data
   local geometry = {112,92}

   local allClasses = {}
   local allFiles = {}

   -- Get number of images to load
   for classdir in lfs.dir(filePath) do
      if lfs.attributes(filePath .. classdir,'mode') == 'directory' and classdir ~= '.' and classdir ~= '..' then
         table.insert(allClasses, classdir)
      end
   end
   table.sort(allClasses)

   for _, classdir in pairs(allClasses) do
      table.insert(allFiles, {})
      for imageFile in lfs.dir(filePath .. classdir) do
         fullImagePath = filePath .. classdir .. '/' .. imageFile
         if lfs.attributes(fullImagePath,'mode') == 'file' then
            table.insert(allFiles[#allFiles], fullImagePath)
         end
      end
      table.sort(allFiles[#allFiles])
   end

   local portionTrain = 0.7
   local numImages = #allFiles * #allFiles[1]
   local train_size = torch.floor(numImages * 0.7)
   local test_size = numImages - train_size

   trainData = {
      data = torch.Tensor(train_size, 1, geometry[1], geometry[2]),
      labels = torch.Tensor(train_size),
      size = function() return train_size end,
      numClasses = function() return #allFiles end,
   }

   testData = {
      data = torch.Tensor(test_size, 1, geometry[1], geometry[2]),
      labels = torch.Tensor(test_size),
      size = function() return test_size end,
      numClasses = function() return #allFiles end,
   }

   -- Actually load the images
   local numClasses = 0
   local numTest = 0
   local numTrain = 0
   for _, class in pairs(allFiles) do
      numClasses = numClasses + 1
      local idxInClass = 0

      for _, imageFile in pairs(class) do
         if #class ~= #trainTestSplit[splitIdx] then
            print("ERROR")
            exit(1)
         end

         idxInClass = idxInClass + 1

         -- print(trainTestSplit[splitIdx][idxInClass] .. " - " .. imageFile)
         if trainTestSplit[splitIdx][idxInClass] == "train" then
               numTrain = numTrain + 1
               trainData.data[numTrain] = image.load(imageFile)
               trainData.labels[numTrain] = numClasses
         else
               numTest = numTest + 1
               testData.data[numTest] = image.load(imageFile)
               testData.labels[numTest] = numClasses
         end
      end
   end
   -- print(numTrain)
   -- print(numTest)
   -- print(numClasses)

   function trainData:normalizeGlobal(mean_, std_)
      local data = self.data
      local std = std_ or data:std()
      local mean = mean_ or data:mean()
      data:add(-mean)
      data:mul(1/std)
      return mean, std
   end

   function testData:normalizeGlobal(mean_, std_)
      local data = self.data
      local std = std_ or data:std()
      local mean = mean_ or data:mean()
      data:add(-mean)
      data:mul(1/std)
      return mean, std
   end

   local labelvector = torch.zeros(#allFiles)

   setmetatable(trainData, {__index = function(self, index)
             local input = self.data[index]
             local class = self.labels[index]
             local label = labelvector:zero()
             label[class] = 1
             local example = {input, label}
                                       return example
   end})

   return trainData, testData, geometry
end
