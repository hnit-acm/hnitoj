<?php
/**
 *默认产生4位数字的验证码
 */
function getVerify($fontfile,$type=1,$length=4,$codeName='verifyCode',$pixel=60,$line=3,$width=200,$height=50){
//创建画布
//$width = 200;
//$height = 50;
$image = imagecreatetruecolor($width,$height);
//创建颜色
$white = imagecolorallocate($image,255,255,255);
//创建填充矩形
imagefilledrectangle($image,0,0,$width,$height,$white);
function getRandColor($image) {
    return imagecolorallocate($image,mt_rand(0,255),mt_rand(0,255),mt_rand(0,255));
}
//$type = 3;
//$length = 4;
switch($type) {
    case 1:
    //数字
        $string = join('',array_rand(range(0,9),$length));
    break;
    case 2:
    //字母
        $string = join('',array_rand(array_flip(array_merge(range('a','z'),range('A','Z'))),$length));
    break;
    case 3:
    //数字+字母
        $string = join('',array_rand(array_flip(array_merge(range(0,9),range('a','z'),range('A','Z'))),$length));
    break;
    case 4:
    //汉字
        $str = "拉,你,哦,是,或";
        $arr = explode(',',$str);
        $string = join('',array_rand(array_flip($arr),$length));
    break;
    default;
    exit('非法参数');
    break;
}
//将验证码存入session
session_start();
$_SESSION[$codeName]=$string;
for($i = 0; $i < $length; $i++) {
    $size = mt_rand(20,28);
    $angle = mt_rand(-15,15);
    $x = 20 + ceil($width/$length)*$i;
    $y = mt_rand(ceil($height/3),$height-20);
    $color = getRandColor($image);
    $text = mb_substr($string,$i,1,'utf-8');
    imagettftext($image,$size,$angle,$x,$y,$color,$fontfile,$text);
}
//添加像素当干扰元素
if($pixel>0) {
    for($i = 0; $i <= $pixel; $i++) {
        imagesetpixel($image,mt_rand(0,$width),mt_rand(0,$height),getRandColor($image));
    }
}
//添加线段当干扰元素
if($line>0) {
    for($i = 0; $i <= $line; $i++) {
        imageline($image,mt_rand(0,$width),mt_rand(0,$height),mt_rand(0,$width),mt_rand(0,$height),getRandColor($image));
    }
}
header('content-type:image/png');
imagepng($image);
imagedestroy($image);
}
