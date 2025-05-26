<?php

namespace Hejunjie\AlipayBillParser;

class CsvExtractor
{

    public function extract(string $zipPath, string $password): array
    {
        $zip = new \ZipArchive();
        if ($zip->open($zipPath) !== true) {
            throw new \RuntimeException("无法打开ZIP文件: $zipPath");
        }
        // 设置密码
        if (!$zip->setPassword($password)) {
            $zip->close();
            throw new \RuntimeException("ZIP密码设置失败");
        }
        // 获取第一个文件名（假设只有一个CSV文件）
        if ($zip->numFiles < 1) {
            $zip->close();
            throw new \RuntimeException("ZIP中无文件");
        }
        $filename = $zip->getNameIndex(0);
        // 临时目录
        $tempDir = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'zip_extract_' . uniqid();
        if (!mkdir($tempDir, 0700) && !is_dir($tempDir)) {
            $zip->close();
            throw new \RuntimeException("创建临时目录失败: $tempDir");
        }
        // 解压第一个文件到临时目录
        if (!$zip->extractTo($tempDir, $filename)) {
            $zip->close();
            rmdir($tempDir);
            throw new \RuntimeException("解压失败");
        }
        $zip->close();

        $csvPath = $tempDir . DIRECTORY_SEPARATOR . $filename;
        if (!file_exists($csvPath)) {
            rmdir($tempDir);
            throw new \RuntimeException("解压文件不存在: $csvPath");
        }
        // 读取CSV内容到数组
        $rows = [];
        $real_name = '';
        $account = '';
        if (($handle = fopen($csvPath, 'r')) !== false) {
            $encoding = null;
            $lineNumber = 0;
            while (($data = fgetcsv($handle)) !== false) {
                $lineNumber++;
                if ($lineNumber < 26) {
                    switch ($lineNumber) {
                        case 3:
                            $encoding = mb_detect_encoding($data[0], ['UTF-8', 'GBK', 'CP936'], true) ?: 'UTF-8';
                            if ($encoding !== 'UTF-8') {
                                $real_name = mb_convert_encoding($data[0], 'UTF-8', $encoding);
                            } else {
                                $real_name = $data[0];
                            }
                            $real_name = str_replace('姓名：', '', $real_name);
                            break;
                        case 4:
                            $encoding = mb_detect_encoding($data[0], ['UTF-8', 'GBK', 'CP936'], true) ?: 'UTF-8';
                            if ($encoding !== 'UTF-8') {
                                $account = mb_convert_encoding($data[0], 'UTF-8', $encoding);
                            } else {
                                $account = $data[0];
                            }
                            $account = str_replace('支付宝账户：', '', $account);
                            break;
                    }
                    continue;
                }
                if ($encoding === null) {
                    $joined = implode(',', array_filter($data, fn($v) => $v !== null && $v !== false));
                    $encoding = mb_detect_encoding($joined, ['UTF-8', 'GBK', 'CP936'], true) ?: 'UTF-8';
                }
                if ($encoding !== 'UTF-8') {
                    $data = array_map(fn($v) => mb_convert_encoding($v, 'UTF-8', $encoding), $data);
                }
                $rows[] = $data;
            }
            fclose($handle);
        } else {
            unlink($csvPath);
            rmdir($tempDir);
            throw new \RuntimeException("打开CSV文件失败");
        }
        // 删除临时文件和目录
        unlink($csvPath);
        rmdir($tempDir);
        return [
            'real_name' => $real_name,
            'account' => $account,
            'data' => $rows
        ];
    }
}
