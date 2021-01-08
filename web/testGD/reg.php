<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta http-equiv="X-UA-Compatible" content="ie=edge">
    <title>注册页面</title>
</head>
<body>
   <h1>注册页面</h1>
   <form action="doAction.php" method="post">
       <table bgcolor="#abcdef" width="80%" border="1" cellspacing="0" cellpadding="0">
           <tr>
               <td>用户名</td>
               <td><input type="text" name="username" id="" placeholder="请输入用户名..."></td>
           </tr>
           <tr>
               <td>密码</td>
               <td><input type="password" name="password" id="" placeholder="请输入密码..."></td>
           </tr>
           <tr>
               <td>验证码</td>
               <td><input type="text" name="verify" id="" placeholder="请输入验证码">
               <img src="getVerify.php" alt="" id="verifyImage"/><a onclick="document.getElementById('verifyImage').src='getVerify.php?r='+Math.random()" href="javascript:void(0)">换一个</a>
               </td>
           </tr>
           <tr>
               <td colspan="2"><input type="submit" value="注册"></td>
           </tr>
       </table>
   </form> 
</body>
</html>