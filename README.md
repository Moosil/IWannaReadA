IWannaReadA is a popup dictionary app inspired by [Yomitan](https://github.com/yomidevs/yomitan), working on your desktop instead of *just* in the browser.

Currently Windows ONLY. Planning to port to Linux/Apple once features are more fleshed out.

## Getting Started
***
### Building from source
Choose a model size for PP-OCR (tiny, small or medium). Medium is recommended
#### Prerequisites
- [Qt6](https://www.qt.io/development/download-qt-installer-oss)
- [OpenCV](https://opencv.org/releases/)
- [OpenMP supported c++ compiler](https://www.openmp.org/resources/openmp-compilers-tools/#compilers) (optional)
#### Linux
```sh
git clone https://github.com/moosil/iwannareada.git
cd iwannareada
git submodule update --init --recursive
curl -o temp.zip https://www.mdbg.net/chinese/export/cedict/cedict_1_0_ts_utf-8_mdbg.zip
unzip temp.zip
rm -f temp.zip
mkdir models
cd models
curl -o preprocessor_config.json "https://huggingface.co/PaddlePaddle/PP-OCRv6_<your chosen model size>_rec_safetensors/resolve/main/preprocessor_config.json?download=true"
grep -o '    "([^"]+)' preprocessor_config.json | cut -d '"' -f 2 > keys.txt
rm -f preprocessor_config.json
curl -o pnnx.zip https://github.com/pnnx/pnnx/releases/download/20260704/pnnx-20260704-linux.zip
unzip pnnx.zip
rm -f pnnx.zip
mv pnnx/pnnx-{$release_number}-windows/pnnx.exe pnnx/pnnx.exe
cd pnnx
curl -o det.onnx "https://huggingface.co/PaddlePaddle/PP-OCRv6_<your chosen model size>_det_onnx/resolve/main/inference.onnx?download=true"
./pnnx.exe ./det.onnx
mv det.ncnn.bin ../det.bin
mv det.ncnn.param ../det.param
curl -o rec.onnx "https://huggingface.co/PaddlePaddle/PP-OCRv6_<your chosen model size>_rec_onnx/resolve/main/inference.onnx?download=true"
./pnnx.exe ./rec.onnx
mv rec.ncnn.bin ../rec.bin
mv rec.ncnn.param ../rec.param
cd ..
rm -rf pnnx
cd ..
mkdir build
cmake -S . -B build
cmake --build build
```

#### Windows
```pwsh
$model_size = "medium"
$ProgressPreference = 'SilentlyContinue'
git clone https://github.com/moosil/iwannareada.git
Set-Location iwannareada
git submodule update --init --recursive
Invoke-WebRequest -o dictionaries.zip https://www.mdbg.net/chinese/export/cedict/cedict_1_0_ts_utf-8_mdbg.zip
Expand-Archive dictionaries.zip
Remove-Item dictionaries.zip -Force
New-Item -ItemType Directory models
Set-Location models
Invoke-WebRequest -OutFile preprocessor_config.json "https://huggingface.co/PaddlePaddle/PP-OCRv6_{$model_size}_rec_safetensors/resolve/main/preprocessor_config.json?download=true"
(Get-Content .\preprocessor_config.json -Encoding utf8 | ConvertFrom-Json).character_list | Out-File ./keys.txt
Remove-Item preprocessor_config.json -Force
$release_number = (Invoke-WebRequest -UseBasicParsing "https://api.github.com/repos/pnnx/pnnx/releases/latest" | ConvertFrom-Json).tag_name
Invoke-WebRequest -OutFile pnnx.zip "https://github.com/pnnx/pnnx/releases/download/{$release_number}/pnnx-{$release_number}-windows.zip"
Expand-Archive pnnx.zip
Remove-Item pnnx.zip -Force
Move-Item "pnnx/pnnx-{$release_number}-windows/pnnx.exe" pnnx/pnnx.exe
Set-Location pnnx
Invoke-WebRequest -OutFile det.onnx "https://huggingface.co/PaddlePaddle/PP-OCRv6_${model_size}_det_onnx/resolve/main/inference.onnx?download=true"
./pnnx.exe ./det.onnx
Move-Item det.ncnn.bin ../det.bin
Move-Item det.ncnn.param ../det.param
Invoke-WebRequest -OutFile rec.onnx "https://huggingface.co/PaddlePaddle/PP-OCRv6_${model_size}_rec_onnx/resolve/main/inference.onnx?download=true"
./pnnx.exe ./rec.onnx
Move-Item rec.ncnn.bin ../rec.bin
Move-Item rec.ncnn.param ../rec.param
Set-Location ..
Remove-Item temp -Recurse -Force
Set-Location ..
New-Item -ItemType Directory build
cmake -S . -B build
cmake --build build
$ProgressPreference = 'Continue'
```